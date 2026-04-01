#include "webSocketServer.h"

WebSocketServer::WebSocketServer(net::io_context& IO_Context, tcp::endpoint endpoint)
    : IO_Context(IO_Context)
    , acceptor(IO_Context) {

    beast::error_code ec;

    // 打开并绑定地址
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("Failed to open acceptor: " + ec.message());
    }

    // 设置端口重用选项
    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("Failed to set reuse_address: " + ec.message());
    }

    // 绑定到端口
    acceptor.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("Failed to bind: " + ec.message());
    }

    // 开始监听
    acceptor.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("Failed to listen: " + ec.message());
    }
}

void WebSocketServer::accept() {
    acceptor.async_accept(
        beast::bind_front_handler(
            &WebSocketServer::onAccept,
            this
        )
    );
}

void WebSocketServer::onAccept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        accept();
        return;
    }

    // 创建会话
    auto session = std::make_shared<WebSocketSession>(std::move(socket));

    // 添加到管理器
    addSession(session->getClientId(), session);
    session->run();

    // 继续接受下一个连接
    accept();
}

void WebSocketServer::addSession(const std::string& clientId,
    std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    sessions[clientId] = session;
}

void WebSocketServer::removeSession(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    sessions.erase(clientId);
}
