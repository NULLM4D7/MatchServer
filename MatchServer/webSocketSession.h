#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <mutex>
#include <boost/asio/steady_timer.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class WebSocketServer;

// 会话类：处理单个 WebSocket 连接 使用文本帧收发消息
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    beast::flat_buffer readBuffer;  // 读取数据缓冲
    websocket::stream<tcp::socket> webSocket;
    std::string clientId;   // 会话的客户端ID

    std::queue<std::shared_ptr<std::string>> sendQueue;    // 发送队列
    std::mutex queueMutex; // 保护队列的互斥锁
	bool isWriting = false;   // 是否正在写入消息 防止重复调用startSend

    // 定时器，每隔一段时间向客户端发送一次心跳请求，若在下次发送前未收到响应则移除当前会话
    net::steady_timer heartbeatTimer;
    bool isTimerRunning = false;
    bool isReceiveHeartbeatRes = false; // 是否收到心跳响应

public:
    static WebSocketServer* webSocketServer;    // 服务器对象 连接关闭时清理服务器对象中的会话容器
    unsigned int roomId; // 所在房间ID
    bool isPlaying = false;   // 是否正在游戏中

    explicit WebSocketSession(tcp::socket socket);
    ~WebSocketSession();

    // 初始化会话（设置超时、最大消息大小），发起 WebSocket 握手
    void run();
    const std::string getClientId() const { return clientId; }
    // 发送消息给客户端
    void sendMessage(const std::string& message);
private:
    // 生成简单的客户端ID
    std::string generateClientId();
	// 握手完成的回调，表示 WebSocket 连接已建立
    void onAccept(beast::error_code ec);
    // 发起异步读取操作，等待客户端发送消息，设置好回调后立即返回
    void readMessage();
	// 读取完成回调，处理收到的消息（获取消息内容、输出日志），响应客户端后再调用 readMessage() 继续等待下一条消息
    void onRead(beast::error_code ec, std::size_t bytes_transferred);

    // 开始发送队列中消息
    void startSend();

    // 定时器回调函数
    void onTimer(const beast::error_code& ec);
    void startTimer();
    void stopTimer();

    // 异步关闭会话
    //void close();

    // 发送心跳请求
    void SendHeartbeatReq();
};