#pragma once
#include <string>

namespace MessageInterpreter
{
	// 消息的第一个字符表示消息类型，最多有2^8=256种消息类型
	enum MessageType
	{
		startMatchReq,	// 开始匹配请求（由客户端发送）
		cancelMatchReq,	// 取消匹配请求（由客户端发送）
		startMatchRes,	// 开始匹配响应（WebSocketSession::onRead中解析到startMatchReq时发送）
		cancelMatchRes,	// 取消匹配响应（WebSocketSession::onRead中解析到cancelMatchReq时发送）
		matchSuccess,	// 匹配成功通知（Room的构造函数中向房间成员发送 消息中需携带UE专用服务器端口）
		matchFailed,	// 匹配失败通知（WebSocketServer::reqMatch中检测到房间人数溢出时，向所有匹配中客户端发送）
		matchInfo,		// 房间内有人请求/取消匹配 服务器向当前房间内所有客户端发送"房间内人数/游戏所需人数"
		heartbeatReq,	// 心跳请求，试探对方是否还活着，若10s内未收到响应则移除会话
		heartbeatRes	// 心跳响应
	};

	std::string messageToString(const MessageType& messageType);

	// 解释客户端发送的消息，并根据消息内容执行相应的操作
	std::string interpret(const std::string& clientId, const std::string& message);
};
