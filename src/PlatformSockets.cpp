#include "PlatformSockets.hpp"
#include <iostream>
#include <stdexcept>

SocketType initServerSocket(const ConfigManager& configManager)
{
#ifdef _WIN32

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData))
        throw std::runtime_error("Winsock startup failed");

    SocketType serverSocket{ socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) };

    if (serverSocket == INVALID_SOCKET)
        throw std::runtime_error("Failed to create server socket");

    bool reuseValue{ true };

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseValue, sizeof(reuseValue)) < 0)
        throw std::runtime_error("Failed to set socket option");

    DWORD timeout{ 10000 };
    if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0)
        throw std::runtime_error("Failed to set socket option");

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(configManager.getPort());

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        throw std::runtime_error("Failed to bind server socket");

    int connectionBacklog{ 10 };

    if (listen(serverSocket, connectionBacklog) < 0)
        throw std::runtime_error("Filed to listen on socket");

#endif

#ifdef __linux__

    SocketType serverSocket{ socket(AF_INET, SOCK_STREAM, 0) };
    if(serverSocket < 0)
        throw std::runtime_error("Failed to create server socket");

    int reuseValue{ 1 };

    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseValue, sizeof(reuseValue)) < 0)
        throw std::runtime_error("Failed to set socket reuse option");

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    if(setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        throw std::runtime_error("Failed to set socket timeour option");

     
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(configManager.getPort());

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        throw std::runtime_error("Failed to bind socket");
    
    int connectionBacklog{ 10 };

    if (listen(serverSocket, connectionBacklog) < 0)
        throw std::runtime_error("Failed to listen on socket");

#endif
    return serverSocket;
}

void closeServerSocket(SocketType serverSocket)
{    
#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#endif

#ifdef __linux__
    close(serverSocket);
#endif
}

void closeClientSocket(SocketType clientSocket)
{
#ifdef _WIN32
    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
#endif

#ifdef __linux__
    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);
#endif  
}

SocketType acceptClientSocket(SocketType serverSocket)
{
    struct sockaddr_in clientAddress;

#ifdef _WIN32
    int clientAdressLength{ sizeof(clientAddress) };
    SocketType clientSocket{ accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAdressLength) };
    if (clientSocket == INVALID_SOCKET)
        throw std::runtime_error("Client attempted to connect but accept() call failed");

    return clientSocket;

#endif

#ifdef __linux__
    socklen_t clientAddressLength{ sizeof(clientAddress) };
    SocketType clientSocket{ accept(serverSocket, (struct sockaddr*)&clientAddress, (socklen_t *) &clientAddressLength) };
    if (clientSocket < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            throw std::runtime_error("Accept timed out");
        }
        
        else
        {
            throw std::runtime_error("Accept failed");
        }
    }


    return clientSocket;
#endif
}

bool checkForTimeout()
{
#ifdef _WIN32
    return WSAGetLastError() == WSAETIMEDOUT;
#endif

#ifdef __linux__
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

std::string getLastError()
{
#ifdef _WIN32
    int errorCode{ WSAGetLastError() };
    char* str{ nullptr };
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&str, 0, nullptr);
    std::string errorMessage{ str ? str : "Unknown Error" };
    LocalFree(str);
    return errorMessage;
#endif

#ifdef __linux__
    return strerror(errno);
#endif
}
