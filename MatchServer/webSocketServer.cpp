#include "webSocketServer.h"
#include "MessageInterpreter.h"
#include "PortChecker.h"
#include <boost/asio.hpp>
#include <fstream>
//#include <chrono>
//#include <iomanip>
//#ifndef _WIN32
//#include <ifaddrs.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <cstring>
//#endif

std::string WebSocketServer::gameServerPath = "~/LinuxServer/EmbersOfTheEndServer.sh";
//std::string WebSocketServer::ip = "127.0.0.1";
int WebSocketServer::playersNumsOfEachRoom = 2;

WebSocketServer::WebSocketServer(net::io_context& IO_Context, tcp::endpoint endpoint)
    : IO_Context(IO_Context)
    , acceptor(IO_Context) {
    //ip = getLocalIP();
    //std::cout << "[WebSocketServer::WebSocketServer] IP:" << ip << std::endl;

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

WebSocketServer::~WebSocketServer()
{
    std::lock_guard<std::mutex> lock(roomsMutex);
    for (auto& roomPair : rooms) {
        roomPair.second->stopUE_Server();  // 直接调用，不依赖析构
    }
    rooms.clear();
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
                roomPair->second->roomSessions.erase(clientId);
                if (roomPair->second->roomSessions.size() == 0)
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
            unsigned int tempRoomId = ++roomId;
            // 创建房间
            std::lock_guard<std::mutex> roomsLock(roomsMutex);
            rooms.insert({ tempRoomId, std::shared_ptr<Room>(new Room(matchingRoom, tempRoomId)) });
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

void WebSocketServer::multicastMatchInfo()
{
    std::stringstream ss;
    ss << static_cast<char>(MessageInterpreter::matchInfo)
        << matchingRoom.size() << "/" << WebSocketServer::playersNumsOfEachRoom;;
    for (const auto& matchingPair : matchingRoom)
    {
        if (matchingPair.second) matchingPair.second->sendMessage(ss.str());
    }
}

//std::string WebSocketServer::getLocalIP() {
//#ifdef _WIN32
//    // Windows 平台：保持原逻辑（或可改用 GetAdaptersInfo 增强）
//    boost::asio::io_context io_context;
//    boost::asio::ip::tcp::resolver resolver(io_context);
//    boost::system::error_code ec;
//    auto endpoints = resolver.resolve(boost::asio::ip::host_name(), "", ec);
//    if (!ec) {
//        for (const auto& endpoint : endpoints) {
//            boost::asio::ip::address addr = endpoint.endpoint().address();
//            if (addr.is_v4() && !addr.is_loopback()) {
//                return addr.to_string();
//            }
//        }
//    }
//    return "127.0.0.1";
//#else
//    // Linux / Unix 平台：枚举网络接口
//    struct ifaddrs* ifaddr = nullptr;
//    if (getifaddrs(&ifaddr) == -1) {
//        return "127.0.0.1";
//    }
//    std::string best_ip = "127.0.0.1";
//    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
//        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
//            continue;
//        // 跳过回环接口
//        if (strcmp(ifa->ifa_name, "lo") == 0)
//            continue;
//        // 可选：跳过常见的虚拟接口（Docker、veth 等）
//        if (strstr(ifa->ifa_name, "docker") != nullptr ||
//            strstr(ifa->ifa_name, "veth") != nullptr)
//            continue;
//        struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
//        char ip[INET_ADDRSTRLEN];
//        inet_ntop(AF_INET, &(addr_in->sin_addr), ip, INET_ADDRSTRLEN);
//        std::string ip_str(ip);
//        // 可选：跳过链路本地地址（169.254.x.x）
//        if (ip_str.find("169.254.") == 0)
//            continue;
//        best_ip = ip_str;
//        break;  // 返回第一个找到的有效 IPv4 地址
//    }
//    freeifaddrs(ifaddr);
//    return best_ip;
//#endif
//}

Room::Room(const std::unordered_map<std::string, std::shared_ptr<WebSocketSession>>& room, unsigned int roomId)
    : roomSessions(room)
    , roomId(roomId)
{
    port = PortChecker::getUsableTCP_Port();
    std::stringstream ss;
    ss << static_cast<char>(MessageInterpreter::MessageType::matchSuccess);

    // 写入端口
    ss << port;
    // 在指定端口启动专用服务器
    startUE_Server(port);
    // 发送匹配成功通知
    for (const auto& sessionPair : room)
    {
        sessionPair.second->sendMessage(ss.str());
        sessionPair.second->isPlaying = true;
		sessionPair.second->roomId = roomId;
	}
}

Room::~Room()
{
    stopUE_Server();
}

void Room::startUE_Server(int port)
{
    // 生成日志文件名
    std::stringstream filename;
    filename << "ueserver_" << port << ".log";

    // 打开日志文件（追加模式）
    logFile = std::make_shared<std::ofstream>(filename.str(), std::ios::out | std::ios::app);
    if (!logFile->is_open()) {
        std::cerr << "Failed to open log file: " << filename.str() << std::endl;
        // 可选：继续运行但不写文件
    }

    serverOutput = std::make_shared<std::string>();
    serverError = std::make_shared<std::string>();

    std::stringstream ss;
    ss << WebSocketServer::gameServerPath << " -PORT=" << port << " -server" << " -IgnoreVersionChecks";

    // 创建进程，在 lambda 中捕获 this 和 logFile
    UE_Process = std::make_unique<TinyProcessLib::Process>(
        ss.str(),
        ".",
        [this, output_buffer = serverOutput, log = logFile](const char* data, size_t size) {
            // 追加到内存缓冲区
            output_buffer->append(data, size);
            // 写入日志文件（线程安全）
            if (log && log->is_open()) {
                std::lock_guard<std::mutex> lock(logMutex);
                log->write(data, size);
                log->flush();   // 可选，保证实时写入
            }
        },
        [this, error_buffer = serverError, log = logFile](const char* data, size_t size) {
            error_buffer->append(data, size);
            std::cerr << "[UE Server Error] " << std::string(data, size);
            if (log && log->is_open()) {
                std::lock_guard<std::mutex> lock(logMutex);
                log->write(data, size);
                log->flush();
            }
        }
    );

    std::cout << "UE Server started on port " << port << " (PID: "
        << (UE_Process ? "running" : "failed") << ")" << std::endl;
}

void Room::stopUE_Server()
{
    if (UE_Process) {
        std::cout << "Terminating UE Server..." << std::endl;
        UE_Process->kill();  // 强制终止进程
        // 注意：kill() 后 process 析构时仍会等待，但此时进程已结束，不会阻塞太久
        UE_Process.reset();
    }
    // 关闭文件流
    if (logFile) {
        logFile->close();
    }
}
