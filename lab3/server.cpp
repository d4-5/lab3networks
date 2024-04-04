#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <cstring>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clientSockets;
std::mutex clientMutex;

void handleClient(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "Received message: " << buffer << std::endl;
        std::lock_guard<std::mutex> lock(clientMutex);
        for (SOCKET socket : clientSockets) {
            if (socket != clientSocket) {
                send(socket, buffer, strlen(buffer), 0);
            }
        }
    }
    // Client disconnected
    closesocket(clientSocket);
    std::lock_guard<std::mutex> lock(clientMutex);
    clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
}

void broadcastServer(const std::string& serverIP, int serverPort) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating server socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());
    serverAddr.sin_port = htons(serverPort);

    if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Error binding server socket: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Error listening on server socket: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "Server broadcasting on " << serverIP << ":" << serverPort << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Error accepting client connection: " << WSAGetLastError() << std::endl;
            continue;
        }
        std::lock_guard<std::mutex> lock(clientMutex);
        clientSockets.push_back(clientSocket);
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
}

int main() {
    std::string serverIP = "127.0.0.1";
    int serverPort = 8888;
    broadcastServer(serverIP, serverPort);
    return 0;
}
