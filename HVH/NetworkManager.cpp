#include "NetworkManager.h"
#include <iostream>

NetworkManager::NetworkManager() {}

NetworkManager::~NetworkManager()
{
    Disconnect();
}

bool NetworkManager::Initialize()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }
    return true;
}

bool NetworkManager::StartServer(int port)
{
    if (role != NetworkRole::NONE) Disconnect();

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    int enable = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed on port " << port << std::endl;
        closesocket(serverSocket);
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(serverSocket);
        return false;
    }

    role = NetworkRole::SERVER;
    running = true;
    networkThread = std::thread(&NetworkManager::ServerThread, this);

    std::cout << "Server started on port " << port << std::endl;
    return true;
}

bool NetworkManager::ConnectToServer(const std::string& ip, int port)
{
    if (role != NetworkRole::NONE) Disconnect();

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << ip << std::endl;
        closesocket(clientSocket);
        return false;
    }

    DWORD timeout = 5000; // 5 
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed to " << ip << ":" << port << std::endl;
        closesocket(clientSocket);
        return false;
    }

    role = NetworkRole::CLIENT;
    connected = true;
    running = true;
    networkThread = std::thread(&NetworkManager::ClientThread, this);

    std::cout << "Connected to server " << ip << ":" << port << std::endl;
    return true;
}

void NetworkManager::Disconnect()
{
    running = false;
    connected = false;

    if (networkThread.joinable()) {
        networkThread.join();
    }

    for (SOCKET socket : clientSockets) {
        closesocket(socket);
    }
    clientSockets.clear();

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    role = NetworkRole::NONE;
    std::cout << "Disconnected from network" << std::endl;
}

void NetworkManager::SendData(const std::string& data)
{
    if (!connected) return;

    if (role == NetworkRole::CLIENT && clientSocket != INVALID_SOCKET) {
        int result = send(clientSocket, data.c_str(), (int)data.size(), 0);
        if (result == SOCKET_ERROR) {
            connected = false;
        }
    }
}

void NetworkManager::BroadcastData(const std::string& data)
{
    if (role == NetworkRole::SERVER) {
        for (SOCKET client : clientSockets) {
            send(client, data.c_str(), (int)data.size(), 0);
        }
    }
}

void NetworkManager::SetDataReceivedCallback(std::function<void(const std::string&, int)> callback)
{
    dataCallback = callback;
}

void NetworkManager::Update()
{
    if (role == NetworkRole::CLIENT && connected) {
        static float pingTimer = 0;
        pingTimer += 0.1f;
        if (pingTimer > 5.0f) {
            pingTimer = 0;
            SendData("PING");
        }
    }
}

void NetworkManager::ServerThread()
{
    while (running) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);

        timeval timeout{ 0, 100000 }; // 100ms

        int activity = select(0, &readSet, nullptr, nullptr, &timeout);
        if (activity > 0 && FD_ISSET(serverSocket, &readSet)) {
            SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                clientSockets.push_back(clientSocket);
                std::thread clientThread(&NetworkManager::HandleClient, this, clientSocket, (int)clientSockets.size() - 1);
                clientThread.detach();

                if (dataCallback) {
                    dataCallback("JOIN", (int)clientSockets.size() - 1);
                }
            }
        }
    }
}

void NetworkManager::ClientThread()
{
    char buffer[1024];

    while (running && connected) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            if (dataCallback) {
                dataCallback(std::string(buffer, bytesReceived), 0);
            }
        }
        else if (bytesReceived == 0) {
            // Connection closed
            connected = false;
            if (dataCallback) {
                dataCallback("DISCONNECT", 0);
            }
            break;
        }
        else {
            // Error or timeout
            if (WSAGetLastError() != WSAETIMEDOUT) {
                connected = false;
                if (dataCallback) {
                    dataCallback("DISCONNECT", 0);
                }
                break;
            }
        }
    }
}

bool NetworkManager::SendToClient(int clientId, const std::string& data)
{
    if (role != NetworkRole::SERVER || clientId < 0 || clientId >= clientSockets.size()) {
        return false;
    }

    if (clientSockets[clientId] != INVALID_SOCKET) {
        int result = send(clientSockets[clientId], data.c_str(), (int)data.size(), 0);
        return result != SOCKET_ERROR;
    }

    return false;
}

void NetworkManager::HandleClient(SOCKET clientSocket, int clientId)
{
    char buffer[1024];

    while (running) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string receivedData(buffer, bytesReceived);

            if (dataCallback) {
                dataCallback(receivedData, clientId);
            }

            if (role == NetworkRole::SERVER) {
                for (size_t i = 0; i < clientSockets.size(); i++) {
                    if (i != clientId && clientSockets[i] != INVALID_SOCKET) {
                        send(clientSockets[i], receivedData.c_str(), receivedData.size(), 0);
                    }
                }
            }
        }
        else {
            if (dataCallback) {
                dataCallback("DISCONNECTED:" + std::to_string(clientId), clientId);
            }

            closesocket(clientSocket);
            if (clientId < clientSockets.size()) {
                clientSockets[clientId] = INVALID_SOCKET;
            }
            break;
        }
    }
}