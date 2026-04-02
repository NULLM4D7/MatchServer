#pragma once
#include <string>

class PortChecker
{
    // 检查TCP端口是否被占用
    static bool isTCP_PortInUse(int port, const std::string& ipAddress = "0.0.0.0");
public:
    // 获取一个可用的TCP端口
    static int getUsableTCP_Port();
};