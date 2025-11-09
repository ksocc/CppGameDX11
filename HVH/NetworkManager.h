#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h> 
#include <windows.h>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

class NetworkManager
{
public:
    enum class NetworkRole { NONE, SERVER, CLIENT };

    NetworkManager();
    ~NetworkManager();

    bool Initialize();
    bool StartServer(int port = 27015);
    bool ConnectToServer(const std::string& ip, int port = 27015);
    void Disconnect();

    void SendData(const std::string& data);
    void BroadcastData(const std::string& data);

    void SetDataReceivedCallback(std::function<void(const std::string&, int)> callback);

    NetworkRole GetRole() const { return role; }
    bool IsConnected() const { return connected; }
    std::vector<std::string> GetConnectedPlayers() const { return connectedPlayers; }

    bool IsServer() const { return role == NetworkRole::SERVER; }
    bool IsClient() const { return role == NetworkRole::CLIENT; }
    int GetClientCount() const { return (int)clientSockets.size(); }

    int GetConnectedClientsCount() const { return (int)clientSockets.size(); }

    bool SendToClient(int clientId, const std::string& data);

    void Update();

private:
    void ServerThread();
    void ClientThread();
    void HandleClient(SOCKET clientSocket, int clientId);

    SOCKET serverSocket = INVALID_SOCKET;
    SOCKET clientSocket = INVALID_SOCKET;

    std::thread networkThread;
    std::atomic<bool> running{ false };
    std::atomic<bool> connected{ false };

    NetworkRole role = NetworkRole::NONE;
    std::vector<SOCKET> clientSockets;
    std::vector<std::string> connectedPlayers;

    std::function<void(const std::string&, int)> dataCallback;
};