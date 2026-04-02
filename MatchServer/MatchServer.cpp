#include "webSocketServer.h"
/*
阶段一：连接建立与握手

函数                          | 用途                                                        | 调用时机
main()                        | 程序入口，创建 I/O 上下文和服务器对象，启动事件循环         | 程序启动时
WebSocketServer::run()        | 启动服务器，调用 accept() 开始监听                          | main 函数中调用
WebSocketServer::accept()     | 发起异步接受连接的操作                                      | 服务器启动时，以及每次接受完一个连接后自动调用
WebSocketServer::onAccept()   | 接受新连接的回调，创建 WebSocketSession 对象处理该连接      | 有客户端连接时
WebSocketSession::run()       | 初始化会话（设置超时、最大消息大小），发起 WebSocket 握手   | 会话创建后立即调用
WebSocketSession::onAccept()  | 握手完成的回调，表示 WebSocket 连接已建立                   | 握手成功时

阶段二：监听客户端消息

函数                            | 用途                                                         | 调用时机
WebSocketSession::readMessage() | 发起异步读取操作，等待客户端发送消息，设置好回调后立即返回   | onAccept 中调用

阶段三：接收客户端消息

函数                        | 用途                                                                                  | 调用时机
WebSocketSession::onRead()  | 读取完成回调，处理收到的消息，响应客户端后再次调用 readMessage() 准备接收下一条消息   | 客户端发送消息后

阶段四：连接关闭

函数                       | 用途                                                                                                         | 调用时机
WebSocketSession::onRead() | 当 ec == websocket::error::closed 等时，表示客户端正常关闭连接，输出断开信息后函数返回（不发起新的读写操作） | 客户端主动关闭连接时


回调链
握手 → onAccept
          ↓ 
       readMessage (注册回调)
          ↓ (客户端发送消息)
       onRead
          ↓ (响应消息并继续读取)
       readMessage (循环)

发送消息 → sendMessage
                ↓ （将消息入队）
            startSend
                ↓ (发送完成)
	        startSend (继续发送队列中下一条消息)
*/
int main(int argc, char* argv[]) {
    try {
        // 检查命令行参数
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(8080);

        std::cout << "[main] Starting WebSocket server on " << address << ":" << port << std::endl;

        // 创建 I/O 上下文
        net::io_context IO_Context{ 1 };

        // 启动房间所需玩家数量 默认为2
        int playersNumsOfEachRoom;
        if (argc > 1) playersNumsOfEachRoom = std::stoi(argv[1]);
        else playersNumsOfEachRoom = 2;

		std::cout << "[main] Players needed to start a room: " << playersNumsOfEachRoom << std::endl;

        // 创建并运行服务器
        WebSocketServer server(IO_Context, tcp::endpoint{ address, port }, playersNumsOfEachRoom);
        WebSocketSession::webSocketServer = &server;
        server.run();

        // 处理 SIGINT 和 SIGTERM 信号以实现关闭
        net::signal_set signals(IO_Context, SIGINT, SIGTERM);
        signals.async_wait([&IO_Context](beast::error_code, int) {
            std::cout << "\n[main] Shutting down server..." << std::endl;
            IO_Context.stop();
            });

        // 运行 I/O 服务
        IO_Context.run();

        std::cout << "[main] Server stopped" << std::endl;
        return EXIT_SUCCESS;

    }
    catch (const std::exception& e) {
        std::cerr << "[main] Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

//#include "PortChecker.h"
//
//int main()
//{
//    for (int i = 0; i < 10; i++) std::cout << PortChecker::getUsableTCP_Port() << std::endl;
//    return 0;
//}