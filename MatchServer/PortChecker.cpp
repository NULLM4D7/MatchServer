#include "PortChecker.h"
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif

bool PortChecker::isTCP_PortInUse(int port, const std::string& ipAddress)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        WSACleanup();
        return false;
    }
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return false;
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress.c_str(), &addr.sin_addr);

    int result = bind(sock, (sockaddr*)&addr, sizeof(addr));

#ifdef _WIN32
    bool isInUse = (result == SOCKET_ERROR && WSAGetLastError() == WSAEADDRINUSE);
    closesocket(sock);
    WSACleanup();
#else
    bool isInUse = (result < 0 && errno == EADDRINUSE);
    close(sock);
#endif

    return isInUse;
}

int PortChecker::getUsableTCP_Port()
{
    static int port = 1023;
    while (true)
    {
        port += 1;
        port %= 65536;
        if (port < 1024) port = 1024;
        if (isTCP_PortInUse(port)) return port;
    }
}
