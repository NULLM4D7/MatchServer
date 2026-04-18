#include "webSocketSession.h"
#include <sstream>
#include "webSocketServer.h"
#include "MessageInterpreter.h"

// 心跳请求时间间隔（秒）
#define HEARTBEAT_INTERVAL 60

WebSocketServer* WebSocketSession::webSocketServer = nullptr;

WebSocketSession::WebSocketSession(tcp::socket socket)
    : webSocket(std::move(socket))
    , clientId(generateClientId())
    , heartbeatTimer(webSocket.get_executor()) {
}

WebSocketSession::~WebSocketSession()
{
    // 停止定时器
    stopTimer();

    if (webSocket.is_open()) {
        beast::error_code ec;
        // 同步关闭
        webSocket.close(websocket::close_code::normal, ec);
        if (ec) {
            std::cerr << "[~WebSocketSession] Close error: " << ec.message() << std::endl;
        }
    }
    std::cout << "[~WebSocketSession] Close session: " << clientId << std::endl;
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

void WebSocketSession::sendMessage(const std::string& message) {
    if (!webSocket.is_open()) {
        std::cerr << "[WebSocketSession::sendMessage] WebSocket not open, cannot send message" << std::endl;
        return;
    }

    // 创建独立缓冲区
    auto buffer = std::make_shared<std::string>(message);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        sendQueue.push(buffer); // 加入输出队列
    }

    char typeChar = static_cast<char>(message[0]);
    std::cout << "[WebSocketSession::sendMessage] Start send message type: " << 
        MessageInterpreter::messageToString(static_cast<MessageInterpreter::MessageType>(typeChar)) << std::endl;
    // 尝试启动发送
    startSend();
}

std::string WebSocketSession::generateClientId() {
    static std::atomic<unsigned int> counter{ 0 };
    return "Client-" + std::to_string(++counter);
}

void WebSocketSession::startSend()
{
    std::shared_ptr<std::string> buffer;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (isWriting || sendQueue.empty()) {   
            // 已经在处理消息发送或消息队列为空
            // 若正在发送消息 继续执行可能导致上一个async_write完成前再次调用async_write 导致断言失败
            return;
        }
        isWriting = true;
        buffer = sendQueue.front();
        sendQueue.pop();
    }

    auto self = shared_from_this();
    webSocket.async_write(
        net::buffer(*buffer),
        [self, buffer](beast::error_code ec, std::size_t bytes) {
            if (ec) {
                std::cerr << "[WebSocketSession::startSend lambda] Send error: " << ec.message() << std::endl;
            }
            {
                std::lock_guard<std::mutex> lock(self->queueMutex);
                self->isWriting = false;
            }
            // 继续发送队列中的下一条
            self->startSend();
        }
    );
}

void WebSocketSession::onAccept(beast::error_code ec) {
    if (ec) {
        std::cerr << "[WebSocketSession::onAccept] Accept error: " << ec.message() << std::endl;
        return;
    }

    std::cout << "[WebSocketSession::onAccept] " << clientId << " connected" << std::endl;

    // 启动定时器
    startTimer();

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
        std::cout << "[WebSocketSession::onRead] " << clientId << " disconnected" << std::endl;
        stopTimer();
        if (webSocketServer) webSocketServer->removeSession(getClientId());

        return;
    }

    if (ec) {
        std::cerr << "[WebSocketSession::onRead] Read error from " << clientId << ": " << ec.message() << std::endl;
        //close();
        if (webSocketServer) webSocketServer->removeSession(getClientId());

        return;
    }

    // 获取接收到的消息
    std::string message = beast::buffers_to_string(readBuffer.data());
    char typeChar = message[0];

    std::cout << "[WebSocketSession::onRead] " << clientId << " send: " 
        << MessageInterpreter::messageToString(static_cast<MessageInterpreter::MessageType>(typeChar)) << std::endl;

    // 接受到心跳响应
    if (typeChar == MessageInterpreter::heartbeatRes)
    {
        isReceiveHeartbeatRes = true;
    }
    else
    {
        // 响应
        sendMessage(MessageInterpreter::interpret(clientId, message));
    }

    readMessage();
}

void WebSocketSession::startTimer() {
    if (isTimerRunning) return;

    isTimerRunning = true;
    auto self = shared_from_this();

    heartbeatTimer.expires_after(std::chrono::seconds(HEARTBEAT_INTERVAL));
    heartbeatTimer.async_wait(
        [self](beast::error_code ec) {
            self->onTimer(ec);
        }
    );

    // 发送心跳包
    SendHeartbeatReq();
}

void WebSocketSession::stopTimer() {
    isTimerRunning = false;
    heartbeatTimer.cancel();
}

void WebSocketSession::onTimer(const beast::error_code& ec) {
    if (ec == net::error::operation_aborted) {
        // 定时器被主动取消
        isTimerRunning = false;
        return;
    }

    if (ec) {
        std::cerr << "[WebSocketSession::onTimer] Timer error: " << ec.message() << std::endl;
        isTimerRunning = false;
        return;
    }

    // 未收到上一次心跳请求的响应，认为客户端死亡，释放当前会话
    if (!isReceiveHeartbeatRes)
    {
        std::cout << "[WebSocketSession::onTimer] Clear session:" << clientId << std::endl;
        //close();
        if (webSocketServer) webSocketServer->removeSession(getClientId());
        return;
    }

    // 重新启动定时器（循环）
    if (isTimerRunning) {
        auto self = shared_from_this();
        heartbeatTimer.expires_after(std::chrono::seconds(HEARTBEAT_INTERVAL));
        heartbeatTimer.async_wait(
            [self](beast::error_code ec) {
                self->onTimer(ec);
            }
        );
    }

    // 发送心跳包
    SendHeartbeatReq();
}

//void WebSocketSession::close()
//{
//    if (!webSocket.is_open()) return;
//    stopTimer();
//    auto self = shared_from_this();
//    webSocket.async_close(
//        websocket::close_code::normal,
//        [self](beast::error_code ec) {
//            if (ec) {
//                std::cerr << "[WebSocketSession::close] Async close error: " << ec.message() << std::endl;
//            }
//            else {
//                std::cout << "[WebSocketSession::close] Connection closed successfully" << std::endl;
//            }
//        }
//    );
//}

void WebSocketSession::SendHeartbeatReq()
{
    std::stringstream ss;
    ss << static_cast<char>(MessageInterpreter::heartbeatReq);
    sendMessage(ss.str());
    isReceiveHeartbeatRes = false;
}