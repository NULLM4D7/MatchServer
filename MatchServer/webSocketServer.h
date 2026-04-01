#pragma once
#include "webSocketSession.h"
#include <unordered_map>
#include <mutex>
#include <set>

class Room;

class WebSocketServer {
private:
    net::io_context& IO_Context;
    tcp::acceptor acceptor;
	// 会话管理器：保存所有活跃会话 key: clientId, value: WebSocketSession 对象
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> sessions;
    std::mutex sessionsMutex;  // 如果多线程访问需要加锁
    // 正在匹配的会话
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> matchingRoom;
	std::mutex matchingRoomMutex;
	// 已经匹配完成 正在游戏中的会话 key: 房间ID value:房间
    std::unordered_map<unsigned int, Room> rooms;
    std::mutex roomsMutex;

	int playersNumsOfEachRoom;  // 每个房间的玩家数量（启动房间所需玩家数量）
public:
    WebSocketServer(net::io_context& IO_Context, tcp::endpoint endpoint, int playersNumsOfEachRoom);

    // 启动服务器，调用 accept() 开始监听
    void run() { accept(); }

    // 添加会话
    void addSession(const std::string& clientId, std::shared_ptr<WebSocketSession> session);
    // 移除会话
    void removeSession(const std::string& clientId);

    // 请求匹配
    void reqMatch(const std::string& clientId);
    // 取消匹配
    void cancelMatch(const std::string& clientId);
private:
    // 发起异步接受连接的操作
    void accept();
    // 接受新连接的回调，创建 WebSocketSession 对象处理该连接
    void onAccept(beast::error_code ec, tcp::socket socket);
};

class Room
{
public:
    // 当前房间中的会话
    std::unordered_map<std::string, std::shared_ptr<WebSocketSession>> roomSessions;
    // 当前房间（UE专用服务器）IP:端口
    std::string address;
    // 当前房间ID
    unsigned int roomId;
    Room(const std::unordered_map<std::string, std::shared_ptr<WebSocketSession>>& room, unsigned int roomId);
};