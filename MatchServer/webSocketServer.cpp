#include "webSocketServer.h"
#include "MessageInterpreter.h"
#include "PortChecker.h"

WebSocketServer::WebSocketServer(net::io_context& IO_Context, tcp::endpoint endpoint, int playersNumsOfEachRoom)
    : IO_Context(IO_Context)
    , acceptor(IO_Context)
    , playersNumsOfEachRoom(playersNumsOfEachRoom){

    beast::error_code ec;

    // 打开并绑定地址
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("[WebSocketServer::WebSocketServer] Failed to open acceptor: " + ec.message());
    }

    // 设置端口重用选项
    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("[WebSocketServer::WebSocketServer] Failed to set reuse_address: " + ec.message());
    }

    // 绑定到端口
    acceptor.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("[WebSocketServer::WebSocketServer] Failed to bind: " + ec.message());
    }

    // 开始监听
    acceptor.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("[WebSocketServer::WebSocketServer] Failed to listen: " + ec.message());
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
        std::cerr << "[WebSocketServer::onAccept] Accept error: " << ec.message() << std::endl;
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
    std::lock_guard<std::mutex> lockSessions(sessionsMutex);
    const auto& sessionPair = sessions.find(clientId);
    // 回收各容器中的会话
    if (sessionPair != sessions.end())
    {
        if (sessionPair->second->isPlaying)
        {
            std::lock_guard<std::mutex> lockRooms(roomsMutex);
            const auto& roomPair = rooms.find(sessionPair->second->roomId);
            if (roomPair != rooms.end())
            {
                roomPair->second.roomSessions.erase(clientId);
                if (roomPair->second.roomSessions.size() == 0)
                {
                    // 该房间的所有玩家都断开连接
                    rooms.erase(roomPair);
                }
            }
        }
        sessions.erase(sessionPair);
        matchingRoom.erase(clientId);
		std::cout << "[WebSocketServer::removeSession] " << clientId << " removed   sessions.size: "
            << sessions.size() << " matchingRoom.size: " << matchingRoom.size() << " rooms.size: " << rooms.size() << std::endl;
    }
    else
    {
		std::cout << "[WebSocketServer::removeSession] Client " << clientId << " not found in sessions" << std::endl;
    }
}

void WebSocketServer::reqMatch(const std::string& clientId)
{
	const auto& res = sessions.find(clientId);
    if (res != sessions.end())
    {
        if (res->second->isPlaying)
        {
            std::cout << "[WebSocketServer::reqMatch] Client " << clientId << " is already in a game, cannot match" << std::endl;
			return;
        }
        std::lock_guard<std::mutex> matchingRoomLock(matchingRoomMutex);
        matchingRoom.insert({ clientId, res->second });
		std::cout << "[WebSocketServer::reqMatch] Current matching players nums: " << matchingRoom.size()
            << "   target nums:" << playersNumsOfEachRoom << std::endl;
        // 人数满足要求，创建房间
        if (matchingRoom.size() == playersNumsOfEachRoom)
        {
            // 生成房间ID
            static std::atomic<unsigned int> roomId{ 0 };
            int tempRoomId = ++roomId;
            // 创建房间
            std::lock_guard<std::mutex> roomsLock(roomsMutex);
            rooms.insert({ tempRoomId, Room(matchingRoom, tempRoomId) });
            // 清空匹配列表
			matchingRoom.clear();
            std::cout << "[WebSocketServer::reqMatch] Create room" << std::endl;
        }
        else if (matchingRoom.size() > playersNumsOfEachRoom)   // 人数溢出
        {
			std::cout << "[WebSocketServer::reqMatch] Matching room overflow: " << matchingRoom.size() << " > " << playersNumsOfEachRoom << std::endl;
            std::stringstream ss;
            ss << static_cast<char>(MessageInterpreter::MessageType::matchFailed);
            for (const auto& room : matchingRoom)
            {
				// 通知所有匹配中的玩家匹配失败
                room.second->sendMessage(ss.str());
            }
            // 清空匹配列表
            matchingRoom.clear();
        }
    }
    else
    {
		std::cout << "[WebSocketServer::reqMatch] Client " << clientId << " not found in sessions" << std::endl;
    }
}

void WebSocketServer::cancelMatch(const std::string& clientId)
{
    const auto& res = sessions.find(clientId);

    if (res != sessions.end())
    {
        std::lock_guard<std::mutex> lock(matchingRoomMutex);
        matchingRoom.erase(clientId);
        std::cout << "[WebSocketServer::cancelMatch] Current matching players nums: " << matchingRoom.size()
            << "   target nums:" << playersNumsOfEachRoom << std::endl;
    }
    else
    {
        std::cout << "[WebSocketServer::cancelMatch] Client " << clientId << " not found in sessions" << std::endl;
    }
}

Room::Room(const std::unordered_map<std::string, std::shared_ptr<WebSocketSession>>& room, unsigned int roomId)
    : roomSessions(room)
    , roomId(roomId)
{
    int port = PortChecker::getUsableTCP_Port();
    // 发送匹配成功通知
    for (const auto& sessionPair : room)
    {
        std::stringstream ss;
        ss << static_cast<char>(MessageInterpreter::MessageType::matchSuccess);
        
        // 写入ip和端口
        ss << "127.0.0.1:" << port;
        // 在指定端口启动专用服务器（待实现）

        sessionPair.second->sendMessage(ss.str());
        sessionPair.second->isPlaying = true;
		sessionPair.second->roomId = roomId;
	}
}
