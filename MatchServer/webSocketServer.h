#pragma once
#include "webSocketSession.h"
#include <unordered_map>
#include <mutex>

class WebSocketServer {
private:
    net::io_context& IO_Context;
    tcp::acceptor acceptor;
    // 会话管理器：保存所有活跃会话
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> sessions;
    std::mutex sessionsMutex;  // 如果多线程访问需要加锁

public:
    WebSocketServer(net::io_context& IO_Context, tcp::endpoint endpoint);

    // 启动服务器，调用 accept() 开始监听
    void run() { accept(); }

    // 添加会话
    void addSession(const std::string& clientId, std::shared_ptr<WebSocketSession> session);
    // 移除会话
    void removeSession(const std::string& clientId);
private:
    // 发起异步接受连接的操作
    void accept();
    // 接受新连接的回调，创建 WebSocketSession 对象处理该连接
    void onAccept(beast::error_code ec, tcp::socket socket);
};