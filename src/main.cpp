#include <iostream>
#include <thread>
#include "PlatformSockets.hpp"

#ifdef __linux__
    #include <sys/socket.h>
#endif

#include "ConfigManager.hpp"
#include "ConnectionManager.hpp"
#include "BufferPool.hpp"

void initWorkerThread(SocketType clientSocket, std::shared_ptr<BufferPool> bufferPool)
{
    //check for tcp handshakes with no follow up data, common in chromium browsers
    char peekBuffer[1];
    int length = recv(clientSocket, peekBuffer, 1, MSG_PEEK);
    
    if (length < 1)
    {
        if (length == 0)
        {
            std::cout << "connection closed on client end\n";
            return;
        }

        if (checkForTimeout())
        {
            std::cout << "Dummy connetion, no data recieved\n";
            closeClientSocket(clientSocket);
            return;
        }

        std::cerr << "Socket error: " << getLastError() << "\n";
        return;
    }


    ConnectionManager connectionManager(clientSocket, bufferPool);
    connectionManager.processConnection();
}

int main()
{
    const ConfigManager& configManager{ ConfigManager::instance() };

    //DEBUG REMOVE
    const std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison>& getMap{ configManager.getGetMap() };
    const std::unordered_map<std::string, Endpoint, TransparentHash, TransparentComparison>& postMap{ configManager.getPostMap() };
    std::cout << "The following endpoints have been populated: \n";
    std::cout << "\n=========GETMAP=========\n";
    for (const auto& [endpointName, endpoint] : getMap)
    {
        std::cout << "Endpoint Name: " << endpointName << "\nType: " << ((endpoint.type == EndpointType::file) ? "file" : "script") << "\nPath: " << endpoint.path << "\n\n";
    }

    std::cout << "\n=========POSTMAP=========\n";
    for (const auto& [endpointName, endpoint] : postMap)
    {
        std::cout << "Endpoint Name: " << endpointName << "\nType: " << ((endpoint.type == EndpointType::file) ? "file" : "script") << "\nPath: " << endpoint.path << "\n\n";
    }
    //END DEBUG

    std::shared_ptr<BufferPool> bufferPool{ std::make_shared<BufferPool>() };

    SocketType serverSocket;

    try
    {
        serverSocket = initServerSocket(configManager);
    }

    catch(const std::exception& e)
    {
        std::cerr << e.what() << " error: " << getLastError() << "\n";
        return -1;
    }

    

    while (true)
    {
        std::cout << "Waiting for a client to connect ...\n";

        SocketType clientSocket;

        try
        {
            clientSocket = acceptClientSocket(serverSocket);
        }

        catch(const std::exception& e)
        {
            if(!checkForTimeout())
                std::cerr << e.what() << " error: " << getLastError();
            continue;
        }

        std::cout << "Client connected\n";
        std::thread worker(initWorkerThread, clientSocket, bufferPool);
        std::cout << "Client passed to worker thread\n";
        worker.detach();
    }

    closeServerSocket(serverSocket);
}
