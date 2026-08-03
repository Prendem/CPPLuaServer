#pragma once
#include "ConfigManager.hpp"

#ifdef _WIN32
	#include <WinSock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "Ws2_32.lib")
	using SocketType = SOCKET;
#endif

#ifdef __linux__
	#include <unistd.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <errno.h>
	#include <cstring>
	using SocketType = int;
#endif

SocketType initServerSocket(const ConfigManager& configManager);
void closeServerSocket(SocketType serverSocket);
void closeClientSocket(SocketType clientSocket);
SocketType acceptClientSocket(SocketType serverSocket);
bool checkForTimeout();
std::string getLastError();


