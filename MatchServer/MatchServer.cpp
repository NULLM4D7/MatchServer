#include "webSocketServer.h"
/*
阶段一：连接建立与握手

| 函数                          | 用途                                                        | 调用时机
| main()                        | 程序入口，创建 I/O 上下文和服务器对象，启动事件循环         | 程序启动时
| WebSocketServer::run()        | 启动服务器，调用 accept() 开始监听                          | main 函数中调用
| WebSocketServer::accept()     | 发起异步接受连接的操作                                      | 服务器启动时，以及每次接受完一个连接后自动调用
| WebSocketServer::onAccept()   | 接受新连接的回调，创建 WebSocketSession 对象处理该连接      | 有客户端连接时
| WebSocketSession::run()       | 初始化会话（设置超时、最大消息大小），发起 WebSocket 握手   | 会话创建后立即调用
| WebSocketSession::onAccept()  | 握手完成的回调，表示 WebSocket 连接已建立，准备发送欢迎消息 | 握手成功时

---

阶段二：发送欢迎消息

| 函数                            | 用途                                                                              | 调用时机
| WebSocketSession::onWrite()     | 数据发送完成的回调，确认欢迎消息已发送，然后调用 readMessage() 准备接收客户端消息 | 欢迎消息发送完成后
| WebSocketSession::readMessage() | 发起异步读取操作，等待客户端发送消息，设置好回调后立即返回                        | onWrite 中调用

关键点：readMessage() 是一个非阻塞函数，它只是注册了异步读取操作就立即返回了，所以日志显示：
- 进入 onWrite → 调用 readMessage → readMessage 立即返回 → 退出 onWrite

---

阶段三：接收客户端消息

| 函数                        | 用途                                                                                            | 调用时机
| WebSocketSession::onRead()  | 读取完成回调，处理收到的消息（获取消息内容、输出日志），然后发起 async_write 将消息回传给客户端 | 客户端发送消息后
| WebSocketSession::onWrite() | 再次被调用，确认回显消息已发送完成，然后再次调用 readMessage() 准备接收下一条消息               | 回显消息发送完成后

循环过程：

客户端发送消息 → onRead 处理 → 发起 async_write(响应) → onWrite 确认 → readMessage 准备接收下一条

---

阶段四：连接关闭

| 函数                       | 用途                                                                                                       | 调用时机
| WebSocketSession::onRead() | 当 ec == websocket::error::closed 时，表示客户端正常关闭连接，输出断开信息后函数返回（不发起新的读写操作） | 客户端主动关闭连接时

---

关键设计模式：异步回调链

实现了一个典型的异步回调链：

[握手] → onAccept
          ↓ (发送欢迎消息)
       onWrite
          ↓ (读取消息)
       readMessage (注册回调)
          ↓ (客户端发送消息)
       onRead
          ↓ (响应消息)
       onWrite
          ↓ (继续读取)
       readMessage (循环)
*/
int main(int argc, char* argv[]) {
    try {
        // 检查命令行参数
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(8080);

        std::cout << "Starting WebSocket server on " << address << ":" << port << std::endl;

        // 创建 I/O 上下文
        net::io_context IO_Context{ 1 };

        // 创建并运行服务器
        WebSocketServer server(IO_Context, tcp::endpoint{ address, port });
        WebSocketSession::webSocketServer = &server;
        server.run();

        // 处理 SIGINT 和 SIGTERM 信号以实现关闭
        net::signal_set signals(IO_Context, SIGINT, SIGTERM);
        signals.async_wait([&IO_Context](beast::error_code, int) {
            std::cout << "\nShutting down server..." << std::endl;
            IO_Context.stop();
            });

        // 运行 I/O 服务
        IO_Context.run();

        std::cout << "Server stopped" << std::endl;
        return EXIT_SUCCESS;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}