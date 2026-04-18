#include "MessageInterpreter.h"
#include <iostream>
#include <sstream>
#include "webSocketSession.h"
#include "webSocketServer.h"

std::string MessageInterpreter::messageToString(const MessageType& messageType)
{
	switch (messageType)
	{
#define CASE(T) case T: return #T
		CASE(startMatchReq);
		CASE(cancelMatchReq);
		CASE(startMatchRes);
		CASE(cancelMatchRes);
		CASE(matchSuccess);
		CASE(matchFailed);
		CASE(matchInfo);
		CASE(heartbeatReq);
		CASE(heartbeatRes);
	default:
		return "Unknown";
	}
}

std::string MessageInterpreter::interpret(const std::string& clientId, const std::string& message)
{
	std::stringstream response;
	// 第一个字符表示消息类型
	switch (message[0])
	{
	case startMatchReq:
	{
		response << static_cast<char>(startMatchRes);
		WebSocketSession::webSocketServer->reqMatch(clientId);
		WebSocketSession::webSocketServer->multicastMatchInfo();
	}
		break;
	case cancelMatchReq:
	{
		response << static_cast<char>(cancelMatchRes);
		WebSocketSession::webSocketServer->cancelMatch(clientId);
		WebSocketSession::webSocketServer->multicastMatchInfo();
	}
		break;
	default:
		std::cout << "[MessageInterpreter::interpret] Unknown message type from " << clientId << ": " << message << std::endl;
		break;
	}
	return response.str();
}
