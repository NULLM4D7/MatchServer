#include "webSocketSession.h"
#include <sstream>
#include "webSocketServer.h"

WebSocketServer* WebSocketSession::webSocketServer = nullptr;

WebSocketSession::WebSocketSession(tcp::socket socket)
    : webSocket(std::move(socket))
    , clientId(generateClientId()) {
}

void WebSocketSession::run() {
    // 设置超时时间（可选）
    webSocket.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server
        )
    );

    // 设置最大消息大小（10MB）
    webSocket.read_message_max(10 * 1024 * 1024);

    // 接受 WebSocket 握手请求
    webSocket.async_accept(
        beast::bind_front_handler(
            &WebSocketSession::onAccept,
            shared_from_this()
        )
    );
}

std::string WebSocketSession::generateClientId() {
    static std::atomic<int> counter{ 0 };
    return "Client-" + std::to_string(++counter);
}

void WebSocketSession::onAccept(beast::error_code ec) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        return;
    }

    std::cout << clientId << " connected" << std::endl;

    writeBuffer = "Welcome to WebSocket Server!";

    // 发送欢迎消息
    webSocket.text(true);  // 设置为文本帧
    webSocket.async_write(
		net::buffer(writeBuffer),   // 不能使用临时变量，因为它会在异步操作完成前被销毁
        beast::bind_front_handler(
            &WebSocketSession::onWrite,
            shared_from_this()
        )
    );
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        std::cerr << "Write error: " << ec.message() << std::endl;
        return;
    }

    // 开始读取消息
    readMessage();
}

void WebSocketSession::readMessage() {
    readBuffer.consume(readBuffer.size());  // 清空缓冲区

    webSocket.async_read(
        readBuffer,
        beast::bind_front_handler(
            &WebSocketSession::onRead,
            shared_from_this()
        )
    );
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == websocket::error::closed ||
        ec == beast::errc::connection_reset ||
        ec == beast::errc::connection_aborted ||
        ec == beast::errc::broken_pipe) {
        // 客户端正常关闭连接
        std::cout << clientId << " disconnected" << std::endl;
        if (webSocketServer) webSocketServer->removeSession(getClientId());
        return;
    }

    if (ec) {
        std::cerr << "Read error from " << clientId << ": " << ec.message() << std::endl;
        if (webSocketServer) webSocketServer->removeSession(getClientId());
        return;
    }

    // 获取接收到的消息
    std::string message = beast::buffers_to_string(readBuffer.data());
    std::cout << clientId << " says: " << message << std::endl;

    // 响应
    std::stringstream ss;
    ss << "ACK:" << message << std::endl;
    writeBuffer = ss.str();

    // 将确认消息发送给客户端
    webSocket.async_write(
        net::buffer(writeBuffer),
        beast::bind_front_handler(
            &WebSocketSession::onWrite,
            shared_from_this()
        )
    );
}