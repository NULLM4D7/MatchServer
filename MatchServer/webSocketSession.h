#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class WebSocketServer;

// 会话类：处理单个 WebSocket 连接 使用文本帧收发消息
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
private:
    websocket::stream<tcp::socket> webSocket;
    beast::flat_buffer readBuffer;  // 读取数据缓冲
    std::string writeBuffer;    // 写入数据缓冲
    std::string clientId;   // 读取会话的客户端ID

public:
    static WebSocketServer* webSocketServer;    // 服务器对象 连接关闭时清理服务器对象中的会话容器

    explicit WebSocketSession(tcp::socket socket);

    // 初始化会话（设置超时、最大消息大小），发起 WebSocket 握手
    void run();
    const std::string getClientId() const { return clientId; }
private:
    // 生成简单的客户端ID
    std::string generateClientId();
	// 握手完成的回调，表示 WebSocket 连接已建立，准备发送欢迎消息
    void onAccept(beast::error_code ec);
    // 数据发送完成的回调，确认欢迎消息已发送，然后调用 readMessage() 准备接收客户端消息
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);
    // 发起异步读取操作，等待客户端发送消息，设置好回调后立即返回
    void readMessage();
    // 读取完成回调，处理收到的消息（获取消息内容、输出日志），然后发起 async_write 响应客户端
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
};