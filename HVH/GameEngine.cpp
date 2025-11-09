#include "GameEngine.h"
#include "SafeRelease.h"
#include "map1.h"
#include "Settings.h"
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#include <mutex>
#include <iphlpapi.h>
#include <vector>
#include <sstream>
#include "Gamemod.h"  
#pragma comment(lib, "iphlpapi.lib")

using namespace DirectX;

std::vector<std::string> GetLocalIPAddresses()
{
    std::vector<std::string> ipAddresses;

    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        return ipAddresses;
    }

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            return ipAddresses;
        }
    }

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            IP_ADDR_STRING* pIpAddrString = &pAdapter->IpAddressList;
            while (pIpAddrString) {
                std::string ip = pIpAddrString->IpAddress.String;
                if (ip != "0.0.0.0" && ip != "127.0.0.1") {
                    if (ip.find('.') != std::string::npos) {
                        if (ip.substr(0, 3) == "26." ||  // Radmin VPN 
                            ip.substr(0, 3) == "27." ||  // Radmin VPN 
                            ip.substr(0, 3) == "10." ||  // local
                            ip.substr(0, 4) == "172." || // local
                            ip.substr(0, 8) == "192.168.") // local
                        {
                            std::string adapterInfo = ip + " (" + pAdapter->Description + ")";
                            ipAddresses.push_back(adapterInfo);
                        }
                    }
                }
                pIpAddrString = pIpAddrString->Next;
            }
            pAdapter = pAdapter->Next;
        }
    }

    free(pAdapterInfo);
    return ipAddresses;
}

GameEngine::GameEngine(HWND window)
    : hWnd(window),
    cameraPosition(0.f, 1.6f, 0.f),
    cameraRotation(0.f, 0.f, 0.f),
    velocity(0.f, 0.f, 0.f),
    wishDir(0.f, 0.f, 0.f),
    acceleration(0.f, 0.f, 0.f),
    health(100.0f),
    gravity(350.0f),
    isGrounded(false),
    flyMode(false),
    thirdPerson(false),
    thirdPersonDistance(5.0f),
    jumpForce(40.0f),
    walkSpeed(50.0f / 3.2f),
    runSpeed(80.0f / 3.2f),
    accelerationRate(90.f),
    friction(4000.0f),
    airAcceleration(45.0f),
    currentSpeed(walkSpeed),
    mouseSensitivity(0.002f),
    buildReach(5.0f),
    consoleActive(false),
    firstMouse(true),
    wasGrounded(false),
    healthBarHeight(0.0f),
    crosshairIndexCount(0),
    healthBarWidth(0.0f),
    healthBarWorldMatrix(),
    healthBarXOffset(0.0f),
    healthBarYOffset(0.0f),
    pLandSound(nullptr),
    pMasterVoice(nullptr),
    pPlayerIB(nullptr),
    pPlayerSRV(nullptr),
    pPlayerVB(nullptr),
    pXAudio2(nullptr),
    playerIndexCount(0),
    playerUpdateTimer(0.0f),
    projectionMatrix(),
    viewMatrix(),
    worldMatrix(),
    blockUpdateTimer(0.1f),
    pInventoryVB(nullptr),
    pInventoryIB(nullptr),
    pInventorySelectVB(nullptr),
    pInventorySelectIB(nullptr),
    inventoryIndexCount(0),
    inventorySelectIndexCount(0),
    gameMode(BUILD_MODE),
    pWhiteTextureSRV(nullptr)
{

    memset(keyProcessed, 0, sizeof(keyProcessed)); 
    memset(mouseStates, 0, sizeof(mouseStates));

    if (!networkManager.Initialize()) {
        MessageBox(hWnd, L"Network initialization failed!", L"Error", MB_OK);
    }

    currentLightDir = XMFLOAT3(0.4f, -0.7f, 0.3f);
    currentLightColor = XMFLOAT3(1.0f, 0.98f, 0.95f);
    currentAmbient = XMFLOAT3(0.2f, 0.2f, 0.25f);

    playerPos = { 0.f, 0.f, 0.f };
    cameraPosition = { playerPos.x, playerPos.y + 1.6f, playerPos.z };
}

void GameEngine::SetGameMode(GameMode mode) {
    gameMode = mode;

    if (gameMode == HVH_MODE) {
        inventoryBlocks.clear();
    }
    else if (gameMode == BUILD_MODE) {
        inventoryBlocks = {
            "grass", "stone", "wood", "metal", "brick", "dirt", "water", "lava", "cube"
        };
    }
}

GameMode GameEngine::GetGameMode() const {
    return gameMode;
}

GameEngine::~GameEngine() { Cleanup(); }

bool GameEngine::Create2DShaders()
{
    HRESULT hr = S_OK;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* err = nullptr;

    const char* vsSource = R"(
        struct VS_INPUT { float2 position : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; };
        struct PS_INPUT { float4 position : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; };
        cbuffer ConstantBuffer : register(b0) { matrix Projection; };
        PS_INPUT main(VS_INPUT i)
        {
            PS_INPUT o;
            o.position = mul(float4(i.position, 0.0f, 1.0f), Projection);
            o.color = i.color;
            o.uv = i.uv;
            return o;
        }
    )";

    const char* psSource = R"(
        Texture2D tex0 : register(t0);
        SamplerState sam0 : register(s0);
        struct PS_INPUT { float4 position : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; };
        float4 main(PS_INPUT i) : SV_Target
        {
            float4 texColor = tex0.Sample(sam0, i.uv);
            return texColor * i.color;
        }
    )";

    hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((char*)err->GetBufferPointer());
        SafeRelease(&err);
        return false;
    }

    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((char*)err->GetBufferPointer());
        SafeRelease(&err);
        SafeRelease(&vsBlob);
        return false;
    }

    hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &p2DVS);
    if (FAILED(hr)) { SafeRelease(&vsBlob); SafeRelease(&psBlob); return false; }

    hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &p2DPS);
    if (FAILED(hr)) { SafeRelease(&vsBlob); SafeRelease(&psBlob); SafeRelease(&p2DVS); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = pDevice->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &p2DInputLayout);
    SafeRelease(&vsBlob);
    SafeRelease(&psBlob);
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(XMMATRIX);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = pDevice->CreateBuffer(&cbd, nullptr, &p2DCB);
    if (FAILED(hr)) return false;

    return true;
}

bool GameEngine::CheckCameraCollision(const XMFLOAT3& cameraPos, float radius) const
{
    AABB cameraAABB;
    cameraAABB.min = {
        cameraPos.x - radius,
        cameraPos.y - radius,
        cameraPos.z - radius
    };
    cameraAABB.max = {
        cameraPos.x + radius,
        cameraPos.y + radius,
        cameraPos.z + radius
    };
    for (const auto& obj : mapObjects)
    {
        AABB blockAABB;
        blockAABB.min = {
            obj.position.x - obj.scale.x,
            obj.position.y - obj.scale.y,
            obj.position.z - obj.scale.z
        };
        blockAABB.max = {
            obj.position.x + obj.scale.x,
            obj.position.y + obj.scale.y,
            obj.position.z + obj.scale.z
        };
        bool intersect =
            cameraAABB.min.x <= blockAABB.max.x &&
            cameraAABB.max.x >= blockAABB.min.x &&
            cameraAABB.min.y <= blockAABB.max.y &&
            cameraAABB.max.y >= blockAABB.min.y &&
            cameraAABB.min.z <= blockAABB.max.z &&
            cameraAABB.max.z >= blockAABB.min.z;

        if (intersect)
        {
            return true;
        }
    }

    return false;
}
void GameEngine::UpdateCamera()
{
    XMVECTOR defaultForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    XMVECTOR upDir = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
    XMVECTOR lookDir = XMVector3TransformCoord(defaultForward, R);
    XMVECTOR eye;

    if (thirdPerson)
    {
        // thirdperson
        XMVECTOR playerPosVec = XMLoadFloat3(&playerPos);
        XMVECTOR desiredEye = XMVectorSubtract(playerPosVec, XMVectorScale(lookDir, thirdPersonDistance));
        desiredEye = XMVectorSetY(desiredEye, XMVectorGetY(desiredEye) + 1.6f);

        XMFLOAT3 desiredEyeF;
        XMStoreFloat3(&desiredEyeF, desiredEye);
        eye = XMLoadFloat3(&desiredEyeF);
    }
    else // firstperson
    {
        XMVECTOR playerPosVec = XMLoadFloat3(&playerPos);
        float eyeHeight = 1.8f;
  
        XMVECTOR baseEye = XMVectorAdd(playerPosVec, XMVectorSet(0.0f, eyeHeight, 0.0f, 0.0f));
        XMVECTOR horizontalLookDir = XMVector3Normalize(XMVectorSet(
            XMVectorGetX(lookDir), 0.0f, XMVectorGetZ(lookDir), 0.0f));
        
        float cameraOffset = 0.3f; 
        XMVECTOR offset = XMVectorScale(horizontalLookDir, cameraOffset);
        
        XMVECTOR desiredEye = XMVectorAdd(baseEye, offset);
        XMFLOAT3 desiredEyeF;
        XMStoreFloat3(&desiredEyeF, desiredEye);

        if (CheckCameraCollision(desiredEyeF))
        {
            eye = baseEye;
        }
        else
        {
            eye = desiredEye;
        }
    }

    XMVECTOR at = XMVectorAdd(eye, lookDir);
    viewMatrix = XMMatrixLookAtLH(eye, at, upDir);
    XMStoreFloat3(&cameraPosition, eye);
}
void GameEngine::Physics(float dt)
{
    float playerHeight = 1.8f;
    float eyeHeight = 1.6f;
    float playerHalfWidth = 0.3f;

    XMFLOAT3 currentVel = velocity;

    bool prevGrounded = isGrounded;

    if (!flyMode)
    {
        if (!isGrounded)
            currentVel.y -= gravity * dt;
        else
            currentVel.y = 0.0f;

        if (isGrounded)
        {
            if (wishDir.x == 0.0f && wishDir.z == 0.0f)
            {
                currentVel.x = 0.0f;
                currentVel.z = 0.0f;
            }
            else
            {
                currentVel.x = wishDir.x * currentSpeed;
                currentVel.z = wishDir.z * currentSpeed;
            }
        }
        else
        {
            float airAccel = airAcceleration * dt;
            float wishSpeed = currentSpeed;

            if (wishDir.x != 0.0f || wishDir.z != 0.0f)
            {
                float projVel = currentVel.x * wishDir.x + currentVel.z * wishDir.z;
                float accelVel = airAccel * currentSpeed;

                if (projVel + accelVel > currentSpeed)
                    accelVel = currentSpeed - projVel;

                if (accelVel > 0.0f)
                {
                    currentVel.x += accelVel * wishDir.x;
                    currentVel.z += accelVel * wishDir.z;
                }
            }
        }
    }
    else
    {
        XMVECTOR pos = XMLoadFloat3(&playerPos);
        XMVECTOR velVec = XMLoadFloat3(&currentVel);
        pos = XMVectorAdd(pos, XMVectorScale(velVec, dt));
        XMStoreFloat3(&playerPos, pos);
        cameraPosition = { playerPos.x, playerPos.y + eyeHeight, playerPos.z };
        velocity = currentVel;
        return;
    }

    float horizontalSpeed = sqrtf(currentVel.x * currentVel.x + currentVel.z * currentVel.z);
    if (horizontalSpeed > currentSpeed)
    {
        float ratio = currentSpeed / horizontalSpeed;
        currentVel.x *= ratio;
        currentVel.z *= ratio;
    }

    XMVECTOR pos = XMLoadFloat3(&playerPos);
    XMVECTOR velVec = XMLoadFloat3(&currentVel);
    XMVECTOR newPos = XMVectorAdd(pos, XMVectorScale(velVec, dt));
    XMFLOAT3 newPosF;
    XMStoreFloat3(&newPosF, newPos);

    AABB playerAABB;
    playerAABB.min = { newPosF.x - playerHalfWidth, newPosF.y, newPosF.z - playerHalfWidth };
    playerAABB.max = { newPosF.x + playerHalfWidth, newPosF.y + playerHeight, newPosF.z + playerHalfWidth };

    bool collisionY = false;
    XMFLOAT3 adjustedPos = newPosF;

    for (const auto& obj : mapObjects)
    {
        AABB blockAABB;
        blockAABB.min = { obj.position.x - obj.scale.x, obj.position.y - obj.scale.y, obj.position.z - obj.scale.z };
        blockAABB.max = { obj.position.x + obj.scale.x, obj.position.y + obj.scale.y, obj.position.z + obj.scale.z };

        bool intersect = playerAABB.min.x <= blockAABB.max.x && playerAABB.max.x >= blockAABB.min.x &&
            playerAABB.min.y <= blockAABB.max.y && playerAABB.max.y >= blockAABB.min.y &&
            playerAABB.min.z <= blockAABB.max.z && playerAABB.max.z >= blockAABB.min.z;

        if (intersect)
        {
            float overlapX1 = playerAABB.max.x - blockAABB.min.x;
            float overlapX2 = blockAABB.max.x - playerAABB.min.x;
            float overlapY1 = playerAABB.max.y - blockAABB.min.y;
            float overlapY2 = blockAABB.max.y - playerAABB.min.y;
            float overlapZ1 = playerAABB.max.z - blockAABB.min.z;
            float overlapZ2 = blockAABB.max.z - playerAABB.min.z;

            float minOverlap = overlapY2;
            int axis = 1;
            if (overlapY1 < minOverlap && overlapY1 > 0) { minOverlap = overlapY1; axis = 1; }
            if (overlapX1 < minOverlap && overlapX1 > 0) { minOverlap = overlapX1; axis = 0; }
            if (overlapX2 < minOverlap && overlapX2 > 0) { minOverlap = overlapX2; axis = 0; }
            if (overlapZ1 < minOverlap && overlapZ1 > 0) { minOverlap = overlapZ1; axis = 2; }
            if (overlapZ2 < minOverlap && overlapZ2 > 0) { minOverlap = overlapZ2; axis = 2; }

            if (axis == 1)
            {
                collisionY = true;

                if (overlapY1 <= overlapY2)
                {
                    adjustedPos.y = blockAABB.min.y - playerHeight - 0.001f;
                    currentVel.y = 0.0f;
                }
                else
                {
                    adjustedPos.y = blockAABB.max.y;
                    currentVel.y = 0.0f;
                    isGrounded = true;
                }
                currentVel.y = 0.0f;
            }
            else if (axis == 0)
            {
                if (overlapX1 <= overlapX2)
                    adjustedPos.x = blockAABB.min.x - playerHalfWidth - 0.001f;
                else
                    adjustedPos.x = blockAABB.max.x + playerHalfWidth + 0.001f;
                currentVel.x = 0.0f;
            }
            else if (axis == 2)
            {
                if (overlapZ1 <= overlapZ2)
                    adjustedPos.z = blockAABB.min.z - playerHalfWidth - 0.001f;
                else
                    adjustedPos.z = blockAABB.max.z + playerHalfWidth + 0.001f;
                currentVel.z = 0.0f;
            }
        }
    }

    const float groundEps = 0.001f;

    if (adjustedPos.y < 0.0f)
    {
        adjustedPos.y = 0.0f;
        currentVel.y = 0.0f;
        isGrounded = true;
    }
    else if (fabsf(adjustedPos.y - 0.0f) <= groundEps && currentVel.y <= 0.0f)
    {
        adjustedPos.y = 0.0f;
        currentVel.y = 0.0f;
        isGrounded = true;
    }
    else if (!collisionY)
    {
        isGrounded = false;
    }

    if (!prevGrounded && isGrounded && pLandSound) {
        pLandSound->Stop();
        pLandSound->FlushSourceBuffers();
        pLandSound->SetVolume(g_Settings.jumpVolume);

        XAUDIO2_BUFFER buffer = {};
        buffer.AudioBytes = sizeof(rawData) - 44;
        buffer.pAudioData = rawData + 44;
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        pLandSound->SubmitSourceBuffer(&buffer);
        pLandSound->Start(0);
    }

    playerPos = adjustedPos;
    velocity = currentVel;

    prevGrounded = isGrounded;

    if (!thirdPerson)
    {
        cameraPosition = { playerPos.x, playerPos.y + eyeHeight, playerPos.z };
    }
}


void GameEngine::RegisterCommands()
{
    commands["exit"] = [this](const auto&) { consoleActive = false; };
    commands["cls"] = [this](const auto&) {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        consoleHistory.clear();
        historyIndex = 0;
        };
    commands["help"] = [this](const auto&) {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        consoleHistory.push_back("Available commands:");
        for (const auto& cmd : commands) {
            consoleHistory.push_back("  " + cmd.first);
        }
        };
    commands["netdebug"] = [this](const auto&) {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("=== Network Debug ===");
        AddToHistory("Multiplayer: " + std::string(isMultiplayer ? "Yes" : "No"));
        AddToHistory("Role: " + std::string(networkManager.IsServer() ? "Server" :
            networkManager.IsClient() ? "Client" : "None"));
        AddToHistory("Connected: " + std::string(networkManager.IsConnected() ? "Yes" : "No"));
        AddToHistory("Players online: " + std::to_string(networkPlayers.size()));
        AddToHistory("Map objects: " + std::to_string(mapObjects.size()));
        };
    commands["host"] = [this](const auto& args) {
        int port = 27015;
        if (args.size() > 1) {
            try { port = std::stoi(args[1]); }
            catch (...) {}
        }

        if (networkManager.StartServer(port)) {
            isMultiplayer = true;
            {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Server started on port " + std::to_string(port));
                AddToHistory("Your IP addresses for connection:");
                auto ipAddresses = GetLocalIPAddresses();
                for (const auto& ip : ipAddresses) {
                    AddToHistory("  " + ip);
                }
                AddToHistory("Players can connect using: 'connect <ip> " + std::to_string(port) + "'");
            }
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Failed to start server");
        }
        };
    commands["ip"] = [this](const auto&) {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("=== Local IP Addresses ===");
        auto ipAddresses = GetLocalIPAddresses();
        if (ipAddresses.empty()) {
            AddToHistory("No network interfaces found");
            AddToHistory("Make sure Radmin VPN is running and connected");
        }
        else {
            for (const auto& ip : ipAddresses) {
                AddToHistory(ip);
            }
        }
        AddToHistory("=== Radmin VPN Usage ===");
        AddToHistory("1. Install and run Radmin VPN");
        AddToHistory("2. Create or join a network");
        AddToHistory("3. Use one of the IPs above (usually 26.x.x.x or 27.x.x.x)");
        AddToHistory("4. Host: 'host' | Client: 'connect <ip>'");
        };
    commands["connect"] = [this](const auto& args) {
        if (args.size() < 2) {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Usage: connect <ip> [port]");
            AddToHistory("Example: connect 26.123.45.67 27015");
            AddToHistory("Use 'ip' command to see available IP addresses");
            return;
        }

        std::string ip = args[1];
        int port = 27015;
        if (args.size() > 2) {
            try { port = std::stoi(args[2]); }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid port number");
                return;
            }
        }

        {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Connecting to " + ip + ":" + std::to_string(port) + "...");
        }

        if (networkManager.ConnectToServer(ip, port)) {
            isMultiplayer = true;
            {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Connected to " + ip + ":" + std::to_string(port));
            }
            networkManager.SendData("JOIN");
            isSynchronized = false;
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Connection failed to " + ip + ":" + std::to_string(port));
            AddToHistory("Check if:");
            AddToHistory("1. Server is running ('host' command)");
            AddToHistory("2. IP address is correct");
            AddToHistory("3. Radmin VPN is connected on both computers");
            AddToHistory("4. Firewall allows connections on port " + std::to_string(port));
        }
        };
    commands["disconnect"] = [this](const auto&) {
        networkManager.SendData("LEAVE");
        networkManager.Disconnect();
        isMultiplayer = false;
        networkPlayers.clear();
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Disconnected from multiplayer");
        };
    commands["players"] = [this](const auto&) {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        if (!isMultiplayer) {
            AddToHistory("Not in multiplayer mode");
            return;
        }
        auto players = networkManager.GetConnectedPlayers();
        AddToHistory("Connected players: " + std::to_string(players.size()));
        for (const auto& player : players) {
            AddToHistory("  " + player);
        }
        };
    commands["fov"] = [this](const auto& args) {
        if (args.size() > 1) {
            try {
                float fov = std::stof(args[1]);
                RECT rc; GetClientRect(hWnd, &rc);
                projectionMatrix = XMMatrixPerspectiveFovLH(
                    XMConvertToRadians(fov),
                    (float)(rc.right - rc.left) / (float)(rc.bottom - rc.top),
                    0.01f, 500.f
                );
            }
            catch (...) {}
        }
        };
    commands["speed"] = [this](const auto& args) {
        if (args.size() > 1) {
            try {
                float newSpeed = std::stof(args[1]);
                walkSpeed = newSpeed;
                currentSpeed = keyStates[VK_SHIFT] ? runSpeed : walkSpeed;
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Walk speed set to: " + args[1]);
            }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid speed value");
            }
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Current walk speed: " + std::to_string(walkSpeed));
        }
        };
    commands["fly"] = [this](const auto&) {
        flyMode = !flyMode;
        velocity = XMFLOAT3(0.f, 0.f, 0.f);
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory(flyMode ? "Fly mode enabled" : "Fly mode disabled");
        };
    commands["gravity"] = [this](const auto& args) {
        if (args.size() > 1) {
            try {
                gravity = std::stof(args[1]);
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Gravity set to: " + args[1]);
            }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid gravity value");
            }
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Current gravity: " + std::to_string(gravity));
        }
        };
    commands["save"] = [this](const auto& args) {
        if (args.size() < 2) {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Usage: save <filename>");
            return;
        }

        std::vector<ParkourObject> parkourObjects;
        for (const auto& obj : mapObjects) {
            ParkourObject pObj;
            pObj.position = obj.position;
            pObj.rotation = obj.rotation;
            pObj.scale = obj.scale;
            pObj.type = obj.type;
            pObj.color = { 1.0f, 1.0f, 1.0f, 1.0f }; // Default color
            parkourObjects.push_back(pObj);
        }

        if (ParkourMap::SaveParkourCourse(parkourObjects, args[1])) {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Map saved to Documents/HVHproject/" + args[1]);
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Failed to save map to Documents/HVHproject/" + args[1]);
        }
        };

    commands["load"] = [this](const auto& args) {
        if (args.size() < 2) {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Usage: load <filename>");
            return;
        }

        std::vector<ParkourObject> parkourObjects = ParkourMap::LoadParkourCourse(args[1]);
        if (parkourObjects.empty()) {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Failed to load map from Documents/HVHproject/" + args[1]);
            return;
        }

        mapObjects.clear();
        for (const auto& pObj : parkourObjects) {
            MapObject obj;
            obj.position = pObj.position;
            obj.rotation = pObj.rotation;
            obj.scale = pObj.scale;
            obj.type = pObj.type;
            mapObjects.push_back(obj);
        }

        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Map loaded from Documents/HVHproject/" + args[1]);

        if (isMultiplayer && networkManager.IsServer()) {
            for (const auto& obj : mapObjects) {
                std::string blockData = "BLOCK:ADD|" +
                    std::to_string(obj.position.x) + "," +
                    std::to_string(obj.position.y) + "," +
                    std::to_string(obj.position.z) + "|" +
                    obj.type;
                networkManager.BroadcastData(blockData);
            }
            AddToHistory("Map sync broadcasted to all clients");
        }
        };
    commands["addcube"] = [this](const auto&) {
        MapObject obj;
        obj.position = { floorf(cameraPosition.x + 0.5f), 1.0f, floorf(cameraPosition.z + 0.5f) };
        obj.rotation = { 0.f, 0.f, 0.f };
        obj.scale = { 1.f, 1.f, 1.f };
        obj.type = "cube";
        mapObjects.push_back(obj);
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Cube added at " +
            std::to_string((int)obj.position.x) + "," +
            std::to_string((int)obj.position.y) + "," +
            std::to_string((int)obj.position.z));
        };
    commands["clear"] = [this](const auto&) {
        mapObjects.clear();
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Map cleared");
        };
    commands["volume"] = [this](const auto& args) {
        if (args.size() > 1) {
            try {
                float volume = std::stof(args[1]);
                SetJumpVolume(volume);
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Jump volume set to: " + std::to_string(volume));
            }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid volume value. Use 0.0 to 1.0");
            }
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Current jump volume: " + std::to_string(GetJumpVolume()));
        }
        };
    commands["restart"] = [this](const auto&) {
        mapObjects.clear();
        parkourObjects = ParkourMap::CreateParkourCourse();
        parkourBalls = ParkourMap::CreateParkourBalls();
        for (const auto& parkourObj : parkourObjects) {
            MapObject obj;
            obj.position = parkourObj.position;
            obj.rotation = parkourObj.rotation;
            obj.scale = parkourObj.scale;
            obj.type = parkourObj.type;
            mapObjects.push_back(obj);
        }
        for (const auto& ball : parkourBalls) {
            MapObject obj;
            obj.position = ball.position;
            obj.rotation = ball.rotation;
            obj.scale = ball.scale;
            obj.type = ball.type;
            mapObjects.push_back(obj);
        }
        playerPos = { 0.f, 0.f, 0.f };
        velocity = { 0.f, 0.f, 0.f };
        isGrounded = false;
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Map reloaded");
        };
    commands["thirdperson"] = [this](const auto&) {
        thirdPerson = !thirdPerson;
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory(thirdPerson ? "Third-person mode enabled" : "First-person mode enabled");
        };
        commands["sky"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                AddToHistory("Usage: sky <parameter> <r> <g> <b>");
                AddToHistory("Parameters: haze, fog_near, fog_far");
                AddToHistory("Examples:");
                AddToHistory("  sky haze 0.7 0.8 0.9");
                AddToHistory("  sky fog_near 255 0 255");
                return;
            }

            std::string param = args[1];

            if (args.size() == 5) {
                try {
                    float r = std::stof(args[2]);
                    float g = std::stof(args[3]);
                    float b = std::stof(args[4]);

                    if (param == "haze") {
                        skybox.SetHazeColor(r, g, b);
                        AddToHistory("Sky haze color set to: " + args[2] + ", " + args[3] + ", " + args[4]);
                    }
                    else if (param == "fog_near") {
                        horizon.SetFogColorNear(r, g, b);
                        AddToHistory("Fog near color set to: " + args[2] + ", " + args[3] + ", " + args[4]);
                    }
                    else if (param == "fog_far") {
                        horizon.SetFogColorFar(r, g, b);
                        AddToHistory("Fog far color set to: " + args[2] + ", " + args[3] + ", " + args[4]);
                    }
                    else {
                        AddToHistory("Error: Unknown parameter '" + param + "'");
                    }
                }
                catch (...) {
                    AddToHistory("Error: Invalid color values. Colors must be numbers (0.0-1.0)");
                }
            }
            else {
                AddToHistory("Error: Wrong number of arguments. Need 3 color values (r g b)");
            }
            };

    commands["sky_default"] = [this](const std::vector<std::string>& args) {
        skybox.SetHorizonColor(0.7f, 0.8f, 0.9f);
        skybox.SetZenithColor(0.1f, 0.2f, 0.4f);
        skybox.SetHazeColor(0.4f, 0.3f, 0.2f);
        horizon.SetFogColorNear(0.95f, 0.95f, 0.98f);
        horizon.SetFogColorFar(0.9f, 0.92f, 1.0f);
        ApplyLighting(
            XMFLOAT3(0.4f, -0.7f, 0.3f),
            XMFLOAT3(1.0f, 0.98f, 0.95f),
            XMFLOAT3(0.2f, 0.2f, 0.25f)
        );
        AddToHistory("Default sky colors and lighting restored");
        };

    commands["world_preset"] = [this](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            AddToHistory("Usage: world_preset <name>");
            AddToHistory("Available presets: default, sunset, storm, day, night, mars, fantasy, arctic, morning, evening, winter, autumn, tropical, desert, cherry, ocean, lavender, candy, neon, moonlight");
            return;
        }

        std::string preset = args[1];

        if (preset == "default") {
            skybox.SetHorizonColor(0.7f, 0.8f, 0.9f);
            skybox.SetZenithColor(0.1f, 0.2f, 0.4f);
            skybox.SetHazeColor(0.4f, 0.3f, 0.2f);
            horizon.SetFogColorNear(0.95f, 0.95f, 0.98f);
            horizon.SetFogColorFar(0.9f, 0.92f, 1.0f);
            ApplyLighting(XMFLOAT3(0.4f, -0.7f, 0.3f), XMFLOAT3(1.0f, 0.98f, 0.95f), XMFLOAT3(0.2f, 0.2f, 0.25f));
            AddToHistory("Default preset applied");
        }
        else if (preset == "sunset") {
            skybox.SetHorizonColor(1.0f, 0.5f, 0.3f);
            skybox.SetZenithColor(0.3f, 0.1f, 0.4f);
            skybox.SetHazeColor(0.8f, 0.4f, 0.2f);
            horizon.SetFogColorNear(1.0f, 0.7f, 0.5f);
            horizon.SetFogColorFar(0.8f, 0.5f, 0.3f);
            ApplyLighting(XMFLOAT3(0.2f, -0.5f, 0.3f), XMFLOAT3(1.0f, 0.6f, 0.4f), XMFLOAT3(0.4f, 0.3f, 0.2f));
            AddToHistory("Sunset preset applied with warm lighting");
        }
        else if (preset == "storm") {
            skybox.SetHorizonColor(0.4f, 0.4f, 0.5f);
            skybox.SetZenithColor(0.1f, 0.1f, 0.2f);
            skybox.SetHazeColor(0.3f, 0.3f, 0.3f);
            horizon.SetFogColorNear(0.5f, 0.5f, 0.6f);
            horizon.SetFogColorFar(0.3f, 0.3f, 0.4f);
            ApplyLighting(XMFLOAT3(0.1f, -0.8f, 0.1f), XMFLOAT3(0.6f, 0.6f, 0.7f), XMFLOAT3(0.2f, 0.2f, 0.25f));
            AddToHistory("Storm preset applied with gloomy lighting");
        }
        else if (preset == "day") {
            skybox.SetHorizonColor(0.7f, 0.8f, 0.9f);
            skybox.SetZenithColor(0.1f, 0.2f, 0.4f);
            skybox.SetHazeColor(0.4f, 0.3f, 0.2f);
            horizon.SetFogColorNear(0.95f, 0.95f, 0.98f);
            horizon.SetFogColorFar(0.9f, 0.92f, 1.0f);
            ApplyLighting(XMFLOAT3(0.4f, -0.7f, 0.3f), XMFLOAT3(1.0f, 0.98f, 0.95f), XMFLOAT3(0.3f, 0.3f, 0.35f));
            AddToHistory("Clear day preset applied with bright lighting");
        }
        else if (preset == "night") {
            skybox.SetHorizonColor(0.1f, 0.1f, 0.2f);
            skybox.SetZenithColor(0.02f, 0.02f, 0.05f);
            skybox.SetHazeColor(0.05f, 0.05f, 0.1f);
            horizon.SetFogColorNear(0.2f, 0.2f, 0.3f);
            horizon.SetFogColorFar(0.1f, 0.1f, 0.2f);
            ApplyLighting(XMFLOAT3(-0.3f, -0.5f, 0.2f), XMFLOAT3(0.3f, 0.4f, 0.8f), XMFLOAT3(0.1f, 0.1f, 0.15f));
            AddToHistory("Night preset applied with moonlight");
        }
        else if (preset == "mars") {
            skybox.SetHorizonColor(0.8f, 0.4f, 0.2f);
            skybox.SetZenithColor(0.4f, 0.2f, 0.1f);
            skybox.SetHazeColor(0.6f, 0.3f, 0.1f);
            horizon.SetFogColorNear(0.9f, 0.5f, 0.3f);
            horizon.SetFogColorFar(0.7f, 0.4f, 0.2f);
            ApplyLighting(XMFLOAT3(0.3f, -0.6f, 0.2f), XMFLOAT3(1.0f, 0.7f, 0.5f), XMFLOAT3(0.4f, 0.2f, 0.1f));
            AddToHistory("Mars preset applied with reddish lighting");
        }
        else if (preset == "fantasy") {
            skybox.SetHorizonColor(0.8f, 0.6f, 1.0f);
            skybox.SetZenithColor(0.3f, 0.1f, 0.6f);
            skybox.SetHazeColor(0.5f, 0.3f, 0.8f);
            horizon.SetFogColorNear(0.9f, 0.7f, 1.0f);
            horizon.SetFogColorFar(0.7f, 0.5f, 0.9f);
            ApplyLighting(XMFLOAT3(0.2f, -0.4f, 0.4f), XMFLOAT3(0.8f, 0.6f, 1.0f), XMFLOAT3(0.3f, 0.2f, 0.4f));
            AddToHistory("Fantasy preset applied with magical lighting");
        }
        else if (preset == "arctic") {
            skybox.SetHorizonColor(0.8f, 0.9f, 1.0f);
            skybox.SetZenithColor(0.4f, 0.6f, 0.8f);
            skybox.SetHazeColor(0.6f, 0.7f, 0.9f);
            horizon.SetFogColorNear(0.95f, 0.97f, 1.0f);
            horizon.SetFogColorFar(0.85f, 0.9f, 1.0f);
            ApplyLighting(XMFLOAT3(0.3f, -0.5f, 0.1f), XMFLOAT3(0.7f, 0.8f, 1.0f), XMFLOAT3(0.4f, 0.5f, 0.6f));
            AddToHistory("Arctic preset applied with cold lighting");
        }
        else if (preset == "morning") {
            skybox.SetHorizonColor(1.0f, 0.7f, 0.5f);
            skybox.SetZenithColor(0.2f, 0.3f, 0.6f);
            skybox.SetHazeColor(0.9f, 0.6f, 0.4f);
            horizon.SetFogColorNear(1.0f, 0.8f, 0.7f);
            horizon.SetFogColorFar(0.8f, 0.7f, 0.9f);
            ApplyLighting(XMFLOAT3(0.1f, -0.3f, 0.2f), XMFLOAT3(1.0f, 0.8f, 0.6f), XMFLOAT3(0.4f, 0.4f, 0.3f));
            AddToHistory("Morning preset applied with soft golden light");
        }
        else if (preset == "evening") {
            skybox.SetHorizonColor(0.9f, 0.4f, 0.2f);
            skybox.SetZenithColor(0.4f, 0.1f, 0.3f);
            skybox.SetHazeColor(0.7f, 0.3f, 0.1f);
            horizon.SetFogColorNear(0.95f, 0.6f, 0.4f);
            horizon.SetFogColorFar(0.8f, 0.4f, 0.3f);
            ApplyLighting(XMFLOAT3(-0.1f, -0.6f, 0.3f), XMFLOAT3(0.9f, 0.5f, 0.3f), XMFLOAT3(0.3f, 0.2f, 0.2f));
            AddToHistory("Evening preset applied with deep orange tones");
        }
        else if (preset == "winter") {
            skybox.SetHorizonColor(0.9f, 0.95f, 1.0f);
            skybox.SetZenithColor(0.5f, 0.7f, 0.9f);
            skybox.SetHazeColor(0.7f, 0.8f, 0.95f);
            horizon.SetFogColorNear(0.98f, 0.98f, 1.0f);
            horizon.SetFogColorFar(0.9f, 0.92f, 1.0f);
            ApplyLighting(XMFLOAT3(0.2f, -0.4f, 0.1f), XMFLOAT3(0.8f, 0.9f, 1.0f), XMFLOAT3(0.5f, 0.6f, 0.7f));
            AddToHistory("Winter preset applied with icy blue tones");
        }
        else if (preset == "autumn") {
            skybox.SetHorizonColor(0.9f, 0.6f, 0.3f);
            skybox.SetZenithColor(0.3f, 0.2f, 0.1f);
            skybox.SetHazeColor(0.7f, 0.5f, 0.2f);
            horizon.SetFogColorNear(0.95f, 0.8f, 0.6f);
            horizon.SetFogColorFar(0.8f, 0.6f, 0.4f);
            ApplyLighting(XMFLOAT3(0.3f, -0.5f, 0.2f), XMFLOAT3(0.9f, 0.7f, 0.4f), XMFLOAT3(0.4f, 0.3f, 0.2f));
            AddToHistory("Autumn preset applied with golden brown tones");
        }
        else if (preset == "tropical") {
            skybox.SetHorizonColor(0.3f, 0.8f, 1.0f);
            skybox.SetZenithColor(0.1f, 0.4f, 0.7f);
            skybox.SetHazeColor(0.2f, 0.6f, 0.9f);
            horizon.SetFogColorNear(0.7f, 0.9f, 1.0f);
            horizon.SetFogColorFar(0.5f, 0.8f, 1.0f);
            ApplyLighting(XMFLOAT3(0.4f, -0.6f, 0.3f), XMFLOAT3(0.6f, 0.9f, 1.0f), XMFLOAT3(0.3f, 0.5f, 0.6f));
            AddToHistory("Tropical preset applied with vibrant blue tones");
        }
        else if (preset == "desert") {
            skybox.SetHorizonColor(1.0f, 0.8f, 0.4f);
            skybox.SetZenithColor(0.6f, 0.4f, 0.2f);
            skybox.SetHazeColor(0.9f, 0.7f, 0.3f);
            horizon.SetFogColorNear(1.0f, 0.9f, 0.7f);
            horizon.SetFogColorFar(0.9f, 0.8f, 0.5f);
            ApplyLighting(XMFLOAT3(0.5f, -0.7f, 0.2f), XMFLOAT3(1.0f, 0.9f, 0.6f), XMFLOAT3(0.5f, 0.4f, 0.3f));
            AddToHistory("Desert preset applied with sandy yellow tones");
        }
        else if (preset == "cherry") {
            skybox.SetHorizonColor(1.0f, 0.7f, 0.8f);
            skybox.SetZenithColor(0.4f, 0.2f, 0.3f);
            skybox.SetHazeColor(0.9f, 0.6f, 0.7f);
            horizon.SetFogColorNear(1.0f, 0.8f, 0.9f);
            horizon.SetFogColorFar(0.9f, 0.7f, 0.8f);
            ApplyLighting(XMFLOAT3(0.2f, -0.4f, 0.3f), XMFLOAT3(1.0f, 0.8f, 0.9f), XMFLOAT3(0.4f, 0.3f, 0.35f));
            AddToHistory("Cherry blossom preset applied with pink tones");
        }
        else if (preset == "ocean") {
            skybox.SetHorizonColor(0.2f, 0.5f, 0.8f);
            skybox.SetZenithColor(0.1f, 0.2f, 0.4f);
            skybox.SetHazeColor(0.15f, 0.35f, 0.6f);
            horizon.SetFogColorNear(0.7f, 0.8f, 1.0f);
            horizon.SetFogColorFar(0.4f, 0.6f, 0.9f);
            ApplyLighting(XMFLOAT3(0.3f, -0.5f, 0.4f), XMFLOAT3(0.5f, 0.7f, 1.0f), XMFLOAT3(0.2f, 0.3f, 0.4f));
            AddToHistory("Ocean deep preset applied with deep blue tones");
        }
        else if (preset == "lavender") {
            skybox.SetHorizonColor(0.7f, 0.6f, 1.0f);
            skybox.SetZenithColor(0.3f, 0.2f, 0.5f);
            skybox.SetHazeColor(0.6f, 0.5f, 0.9f);
            horizon.SetFogColorNear(0.9f, 0.8f, 1.0f);
            horizon.SetFogColorFar(0.8f, 0.7f, 1.0f);
            ApplyLighting(XMFLOAT3(0.2f, -0.4f, 0.3f), XMFLOAT3(0.8f, 0.7f, 1.0f), XMFLOAT3(0.3f, 0.25f, 0.4f));
            AddToHistory("Lavender field preset applied with purple tones");
        }
        else if (preset == "candy") {
            skybox.SetHorizonColor(1.0f, 0.9f, 0.95f);
            skybox.SetZenithColor(0.8f, 0.6f, 0.9f);
            skybox.SetHazeColor(0.95f, 0.8f, 0.9f);
            horizon.SetFogColorNear(1.0f, 0.95f, 1.0f);
            horizon.SetFogColorFar(0.95f, 0.9f, 1.0f);
            ApplyLighting(XMFLOAT3(0.1f, -0.3f, 0.2f), XMFLOAT3(1.0f, 0.95f, 1.0f), XMFLOAT3(0.4f, 0.35f, 0.45f));
            AddToHistory("Candy land preset applied with pastel colors");
        }
        else if (preset == "neon") {
            skybox.SetHorizonColor(0.3f, 1.0f, 0.8f);
            skybox.SetZenithColor(0.1f, 0.3f, 0.6f);
            skybox.SetHazeColor(0.2f, 0.8f, 0.7f);
            horizon.SetFogColorNear(0.6f, 1.0f, 0.9f);
            horizon.SetFogColorFar(0.3f, 0.8f, 0.7f);
            ApplyLighting(XMFLOAT3(0.1f, -0.5f, 0.4f), XMFLOAT3(0.4f, 1.0f, 0.8f), XMFLOAT3(0.1f, 0.3f, 0.25f));
            AddToHistory("Neon cyberpunk preset applied with electric colors");
        }
        else if (preset == "moonlight") {
            skybox.SetHorizonColor(0.15f, 0.15f, 0.3f);
            skybox.SetZenithColor(0.05f, 0.05f, 0.1f);
            skybox.SetHazeColor(0.1f, 0.1f, 0.2f);
            horizon.SetFogColorNear(0.25f, 0.25f, 0.4f);
            horizon.SetFogColorFar(0.15f, 0.15f, 0.3f);
            ApplyLighting(XMFLOAT3(-0.2f, -0.8f, 0.1f), XMFLOAT3(0.2f, 0.3f, 0.6f), XMFLOAT3(0.05f, 0.05f, 0.1f));
            AddToHistory("Moonlight preset applied with deep night tones");
        }
        else {
            AddToHistory("Error: Unknown preset '" + preset + "'");
            AddToHistory("Available presets: default, sunset, storm, day, night, mars, fantasy, arctic, morning, evening, winter, autumn, tropical, desert, cherry, ocean, lavender, candy, neon, moonlight");
        }
    };

    commands["sky_default"] = [this](const std::vector<std::string>& args) {
    skybox.SetHorizonColor(0.7f, 0.8f, 0.9f);
    skybox.SetZenithColor(0.1f, 0.2f, 0.4f);
    skybox.SetHazeColor(0.4f, 0.3f, 0.2f);
    horizon.SetFogColorNear(0.95f, 0.95f, 0.98f);
    horizon.SetFogColorFar(0.9f, 0.92f, 1.0f);
    
    ApplyLighting( XMFLOAT3(0.4f, -0.7f, 0.3f), XMFLOAT3(1.0f, 0.98f, 0.95f), XMFLOAT3(0.2f, 0.2f, 0.25f));
    
    AddToHistory("Default sky colors and lighting restored");
};


    commands["cloud_custom"] = [this](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            AddToHistory("Usage: cloud_custom <parameter> <values>");
            AddToHistory("Parameters: fog_near, fog_far");
            AddToHistory("Examples:");
            AddToHistory("  cloud_custom fog_near 0.95 0.95 0.98");
            AddToHistory("  cloud_custom fog_far 255 255 128");
            return;
        }

        std::string param = args[1];

        if ((param == "fog_near" || param == "fog_far") && args.size() == 5) {
            try {
                float r = std::stof(args[2]);
                float g = std::stof(args[3]);
                float b = std::stof(args[4]);

                if (param == "fog_near") {
                    horizon.SetFogColorNear(r, g, b);
                    AddToHistory("Fog near color set to: " + args[2] + ", " + args[3] + ", " + args[4]);
                }
                else if (param == "fog_far") {
                    horizon.SetFogColorFar(r, g, b);
                    AddToHistory("Fog far color set to: " + args[2] + ", " + args[3] + ", " + args[4]);
                }
            }
            catch (...) {
                AddToHistory("Error: Invalid color values. Colors must be numbers (0.0-1.0)");
            }
        }
        else if (args.size() > 2) {
            try {
                float value = std::stof(args[2]);

                if (param == "radius") {
                    // horizon.SetRadius(value);
                    AddToHistory("Scene radius set to: " + args[2]);
                }
                else if (param == "speed") {
                    // horizon.SetSpeed(value);
                    AddToHistory("Scene speed set to: " + args[2]);
                }
                else {
                    AddToHistory("Error: Unknown cloud parameter '" + param + "'");
                }
            }
            catch (...) {
                AddToHistory("Error: Invalid value. Must be a number");
            }
        }
        else {
            AddToHistory("Error: Wrong number of arguments");
        }
        };
   
    commands["scene_default"] = [this](const std::vector<std::string>& args) {
        skybox.SetHorizonColor(0.7f, 0.8f, 0.9f);
        skybox.SetZenithColor(0.1f, 0.2f, 0.4f);
        skybox.SetHazeColor(0.4f, 0.3f, 0.2f);
        horizon.SetFogColorNear(0.95f, 0.95f, 0.98f);
        horizon.SetFogColorFar(0.9f, 0.92f, 1.0f);

        AddToHistory("Default scene settings restored");
        };

    commands["inventory"] = [this](const auto&) {
        inventoryOpen = !inventoryOpen;
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory(inventoryOpen ? "Creative inventory opened" : "Creative inventory closed");
        };
}


void GameEngine::ProcessCommand(const std::string& command)
{
    if (command.empty()) return;

    AddToHistory("> " + command);

    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;

    while (iss >> arg) {
        args.push_back(arg);
    }

    if (args.empty()) return;

    auto it = commands.find(args[0]);
    if (it != commands.end()) {
        it->second(args);
    }
    else {
        AddToHistory("Unknown command: " + args[0]);
    }
}

void GameEngine::AddToHistory(const std::string& command)
{
    try {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        consoleHistory.push_back(command);
        if (consoleHistory.size() > maxConsoleHistorySize) {
            consoleHistory.erase(consoleHistory.begin());
        }
        historyIndex = consoleHistory.size();
    }
    catch (const std::system_error& e) {
        std::string errorMsg = "Mutex error in AddToHistory: " + std::string(e.what());
        OutputDebugStringA(errorMsg.c_str());
    }
}

void GameEngine::Update(float dt)
{
    ResetKeyProcessing();

    XMFLOAT3 oldPos = playerPos;
    HandleInput(dt);
    HandleBuilding();
    UpdateCamera();

    playerVelocity = {
        (playerPos.x - oldPos.x) / dt,
        (playerPos.y - oldPos.y) / dt,
        (playerPos.z - oldPos.z) / dt
    };

    if (isMultiplayer) {
        networkUpdateTimer += dt;
        if (networkUpdateTimer > 0.2f) {
            networkUpdateTimer = 0;
            int myId = networkManager.IsServer() ? -1 : 0;
            std::string playerData = "POS:" + std::to_string(myId) + ":" +
                std::to_string(playerPos.x) + "," +
                std::to_string(playerPos.y) + "," +
                std::to_string(playerPos.z);

            if (networkManager.IsServer()) {
                networkManager.BroadcastData(playerData);
            }
            else {
                networkManager.SendData(playerData);
            }
        }
        networkManager.Update();
    }
}
bool GameEngine::IsBlockAtPosition(const XMFLOAT3& position, float tolerance) const
{
    for (const auto& obj : mapObjects) {
        if (fabs(obj.position.x - position.x) < tolerance &&
            fabs(obj.position.y - position.y) < tolerance &&
            fabs(obj.position.z - position.z) < tolerance) {
            return true;
        }
    }
    return false;
}

void GameEngine::SetJumpVolume(float volume) {
    g_Settings.jumpVolume = clamp(volume, 0.0f, 1.0f);
    if (pLandSound) {
        pLandSound->SetVolume(g_Settings.jumpVolume);
    }
}

float GameEngine::GetJumpVolume() const {
    return g_Settings.jumpVolume;
}


void GameEngine::PlaceBlock(const XMFLOAT3& position)
{
    float playerHeight = 1.8f;
    float playerHalfWidth = 0.3f;

    AABB newBlockAABB;
    newBlockAABB.min = { position.x - 0.5f, position.y - 0.5f, position.z - 0.5f };
    newBlockAABB.max = { position.x + 0.5f, position.y + 0.5f, position.z + 0.5f };

    AABB playerAABB;
    playerAABB.min = { playerPos.x - playerHalfWidth, playerPos.y, playerPos.z - playerHalfWidth };
    playerAABB.max = { playerPos.x + playerHalfWidth, playerPos.y + playerHeight, playerPos.z + playerHalfWidth };

    bool intersectsPlayer = (playerAABB.min.x <= newBlockAABB.max.x && playerAABB.max.x >= newBlockAABB.min.x &&
        playerAABB.min.y <= newBlockAABB.max.y && playerAABB.max.y >= newBlockAABB.min.y &&
        playerAABB.min.z <= newBlockAABB.max.z && playerAABB.max.z >= newBlockAABB.min.z);

    if (!intersectsPlayer) {
        MapObject obj;
        obj.position = position;
        obj.rotation = { 0.f, 0.f, 0.f };
        obj.scale = { 1.f, 1.f, 1.f };
        obj.type = inventoryBlocks[selectedInventorySlot];
        mapObjects.push_back(obj);

        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Block placed at " +
            std::to_string((int)position.x) + "," +
            std::to_string((int)position.y) + "," +
            std::to_string((int)position.z));

        if (isMultiplayer && isSynchronized) {
            std::string blockData = "BLOCK:ADD|" +
                std::to_string(position.x) + "," +
                std::to_string(position.y) + "," +
                std::to_string(position.z) + "|" +
                inventoryBlocks[selectedInventorySlot];
            if (networkManager.IsServer()) {
                networkManager.BroadcastData(blockData);
            }
            else {
                networkManager.SendData(blockData);
            }
        }
    }
    else {
        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Cannot place block - collision with player");
    }
}

bool GameEngine::Initialize()
{
    mouseLocked = true;
    ShowCursor(FALSE);

    networkManager.Initialize();

    networkManager.SetDataReceivedCallback([this](const std::string& data, int clientId) {
        {
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            //AddToHistory("Received: " + data + " from " + std::to_string(clientId));
        }

        if (data == "MAP:CLEAR" && !networkManager.IsServer()) {
            mapObjects.clear();
            std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
            AddToHistory("Client map cleared by server");
        }
        else if (data.find("POS:") == 0) {
            std::string posStr = data.substr(4);
            size_t colon1 = posStr.find(':');
            if (colon1 != std::string::npos) {
                std::string idStr = posStr.substr(0, colon1);
                std::string coordStr = posStr.substr(colon1 + 1);

                try {
                    int senderId = std::stoi(idStr);

                    size_t comma1 = coordStr.find(',');
                    size_t comma2 = coordStr.find(',', comma1 + 1);
                    if (comma1 != std::string::npos && comma2 != std::string::npos) {
                        float x = std::stof(coordStr.substr(0, comma1));
                        float y = std::stof(coordStr.substr(comma1 + 1, comma2 - comma1 - 1));
                        float z = std::stof(coordStr.substr(comma2 + 1));

                        if (networkPlayers.find(senderId) == networkPlayers.end()) {
                            NetworkPlayer newPlayer;
                            newPlayer.clientId = senderId;
                            newPlayer.name = "Player" + std::to_string(senderId);
                            newPlayer.position = XMFLOAT3(x, y, z);
                            networkPlayers[senderId] = newPlayer;
                            {
                                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                                AddToHistory("Player " + std::to_string(senderId) + " added/updated");
                            }
                        }
                        else {
                            networkPlayers[senderId].position = XMFLOAT3(x, y, z);
                            {
                                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                                //AddToHistory("Position updated for player " + std::to_string(senderId));
                            }
                        }

                        if (networkManager.IsServer()) {
                            networkManager.BroadcastData(data);
                        }
                    }
                }
                catch (...) {
                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                    AddToHistory("Invalid position data");
                }
            }
        }
        else if (data.find("BLOCK:ADD|") == 0) {
            std::string blockStr = data.substr(10);
            size_t pipePos = blockStr.find('|');

            if (pipePos != std::string::npos) {
                std::string posStr = blockStr.substr(0, pipePos);
                std::string type = blockStr.substr(pipePos + 1);

                size_t comma1 = posStr.find(',');
                size_t comma2 = posStr.find(',', comma1 + 1);

                if (comma1 != std::string::npos && comma2 != std::string::npos) {
                    try {
                        float x = std::stof(posStr.substr(0, comma1));
                        float y = std::stof(posStr.substr(comma1 + 1, comma2 - comma1 - 1));
                        float z = std::stof(posStr.substr(comma2 + 1));

                        bool blockExists = false;
                        for (const auto& obj : mapObjects) {
                            if (fabs(obj.position.x - x) < 0.1f &&
                                fabs(obj.position.y - y) < 0.1f &&
                                fabs(obj.position.z - z) < 0.1f) {
                                blockExists = true;
                                break;
                            }
                        }

                        if (!blockExists) {
                            MapObject newBlock;
                            newBlock.position = XMFLOAT3(x, y, z);
                            newBlock.rotation = XMFLOAT3(0.f, 0.f, 0.f);
                            newBlock.scale = XMFLOAT3(1.f, 1.f, 1.f);
                            newBlock.type = type;
                            mapObjects.push_back(newBlock);

                            if (networkManager.IsServer()) {
                                networkManager.BroadcastData(data);
                                {
                                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                                    //AddToHistory("Block added and broadcasted");
                                }
                            }
                            else {
                                {
                                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                                    //AddToHistory("Block added by network at (" +
                                    //    std::to_string(x) + "," +
                                    //    std::to_string(y) + "," +
                                    //    std::to_string(z) + ")");
                                }
                            }
                        }
                    }
                    catch (...) {
                        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                        AddToHistory("Invalid block data format");
                    }
                }
            }
        }
        else if (data.find("BLOCK:REMOVE|") == 0) {
            std::string posStr = data.substr(13);
            size_t comma1 = posStr.find(',');
            size_t comma2 = posStr.find(',', comma1 + 1);

            if (comma1 != std::string::npos && comma2 != std::string::npos) {
                try {
                    float x = std::stof(posStr.substr(0, comma1));
                    float y = std::stof(posStr.substr(comma1 + 1, comma2 - comma1 - 1));
                    float z = std::stof(posStr.substr(comma2 + 1));

                    bool blockRemoved = false;
                    for (auto it = mapObjects.begin(); it != mapObjects.end(); ) {
                        if (fabs(it->position.x - x) < 0.1f &&
                            fabs(it->position.y - y) < 0.1f &&
                            fabs(it->position.z - z) < 0.1f) {
                            it = mapObjects.erase(it);
                            blockRemoved = true;

                            if (networkManager.IsServer()) {
                                networkManager.BroadcastData(data);
                                {
                                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                                    AddToHistory("Block removal broadcasted to all clients");
                                }
                            }
                            else {
                                {
                                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                                    AddToHistory("Block removed by network at (" +
                                        std::to_string(x) + "," +
                                        std::to_string(y) + "," +
                                        std::to_string(z) + ")");
                                }
                            }
                            break;
                        }
                        else {
                            ++it;
                        }
                    }

                    if (!blockRemoved) {
                        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                        AddToHistory("Block not found for removal");
                    }
                }
                catch (...) {
                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                    AddToHistory("Invalid remove data format");
                }
            }
        }
        else if (data == "JOIN") {
            if (networkManager.IsServer()) {
                int assignedId = nextClientId++;
                {
                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                    AddToHistory("Player joined, assigned ID: " + std::to_string(assignedId));
                }

                // Send client ID
                std::string idData = "ASSIGN_ID:" + std::to_string(assignedId);
                networkManager.SendToClient(clientId, idData);

                networkManager.SendToClient(clientId, "MAP:CLEAR");

                // Send server map to client
                for (const auto& obj : mapObjects) {
                    std::string blockData = "BLOCK:ADD|" +
                        std::to_string(obj.position.x) + "," +
                        std::to_string(obj.position.y) + "," +
                        std::to_string(obj.position.z) + "|" +
                        obj.type;
                    networkManager.SendToClient(clientId, blockData);
                }

                // Send existing players pos
                for (const auto& pair : networkPlayers) {
                    std::string posData = "POS:" + std::to_string(pair.first) + ":" +
                        std::to_string(pair.second.position.x) + "," +
                        std::to_string(pair.second.position.y) + "," +
                        std::to_string(pair.second.position.z);
                    networkManager.SendToClient(clientId, posData);
                }

                // Notify other clients about new player
                std::string newPlayerData = "NEW_PLAYER:" + std::to_string(assignedId);
                networkManager.BroadcastData(newPlayerData);
            }
        }
        else if (data == "LEAVE") {
            if (networkManager.IsServer()) {
                networkPlayers.erase(clientId);
                {
                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                    AddToHistory("Player " + std::to_string(clientId) + " left");
                }

                networkManager.BroadcastData("PLAYER_LEFT:" + std::to_string(clientId));
            }
        }
        else if (data.find("PLAYER_LEFT:") == 0) {
            std::string idStr = data.substr(12);
            try {
                int leftId = std::stoi(idStr);
                networkPlayers.erase(leftId);
                {
                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                    AddToHistory("Player " + std::to_string(leftId) + " disconnected");
                }
            }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid player leave message");
            }
        }
        else if (data.find("ASSIGN_ID:") == 0 && !networkManager.IsServer()) {
            std::string idStr = data.substr(10);
            try {
                int assignedId = std::stoi(idStr);
                {
                    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                    AddToHistory("Assigned client ID: " + std::to_string(assignedId));
                }
                isSynchronized = true; // Synchronization complete after receiving ID
            }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid assign ID data");
            }
        }
        else if (data.find("NEW_PLAYER:") == 0) {
            std::string idStr = data.substr(11);
            try {
                int newId = std::stoi(idStr);
                if (networkPlayers.find(newId) == networkPlayers.end()) {
                    NetworkPlayer newPlayer;
                    newPlayer.clientId = newId;
                    newPlayer.name = "Player" + std::to_string(newId);
                    newPlayer.position = XMFLOAT3(0.f, 0.f, 0.f);
                    networkPlayers[newId] = newPlayer;
                    {
                        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                        AddToHistory("New player " + std::to_string(newId) + " joined");
                    }
                }
            }
            catch (...) {
                std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
                AddToHistory("Invalid new player data");
            }
        }
        });

        HRESULT hr = S_OK;

        D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0 };
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, fls, 1,
            D3D11_SDK_VERSION, &pDevice, nullptr, &pContext);
        if (FAILED(hr)) return false;

        RECT rc; GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 200;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 4;
        sd.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGIDevice* pDXGIDevice = nullptr;
        hr = pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice);
        if (FAILED(hr)) return false;
        IDXGIAdapter* pAdapter = nullptr; pDXGIDevice->GetAdapter(&pAdapter);
        IDXGIFactory* pFactory = nullptr; pAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pFactory);
        hr = pFactory->CreateSwapChain(pDevice, &sd, &pSwapChain);
        SafeRelease(&pFactory); SafeRelease(&pAdapter); SafeRelease(&pDXGIDevice);
        if (FAILED(hr)) return false;

        ID3D11Texture2D* pBack = nullptr;
        hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBack);
        if (FAILED(hr)) return false;
        hr = pDevice->CreateRenderTargetView(pBack, nullptr, &pRTV);
        SafeRelease(&pBack);
        if (FAILED(hr)) return false;

        ID3D11Texture2D* pDepth = nullptr;
        D3D11_TEXTURE2D_DESC dtd = {};
        dtd.Width = width; dtd.Height = height; dtd.MipLevels = 1; dtd.ArraySize = 1;
        dtd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dtd.SampleDesc.Count = 4;
        dtd.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
        dtd.Usage = D3D11_USAGE_DEFAULT;
        dtd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        hr = pDevice->CreateTexture2D(&dtd, nullptr, &pDepth);
        if (FAILED(hr)) return false;
        hr = pDevice->CreateDepthStencilView(pDepth, nullptr, &pDSV);
        SafeRelease(&pDepth);
        if (FAILED(hr)) return false;

        pContext->OMSetRenderTargets(1, &pRTV, pDSV);

        D3D11_VIEWPORT vp{};
        vp.Width = (float)width; vp.Height = (float)height;
        vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
        pContext->RSSetViewports(1, &vp);

        D3D11_RASTERIZER_DESC rasterDesc = {};
        rasterDesc.FillMode = D3D11_FILL_SOLID;
        rasterDesc.CullMode = D3D11_CULL_BACK;  
        rasterDesc.FrontCounterClockwise = false;
        rasterDesc.DepthBias = 0;
        rasterDesc.DepthBiasClamp = 0.0f;
        rasterDesc.SlopeScaledDepthBias = 0.0f;
        rasterDesc.DepthClipEnable = true;
        rasterDesc.ScissorEnable = false;
        rasterDesc.MultisampleEnable = false;
        rasterDesc.AntialiasedLineEnable = false;

        hr = pDevice->CreateRasterizerState(&rasterDesc, &pRasterState);
        if (FAILED(hr)) return false;

        pContext->RSSetState(pRasterState);

        // D3D init true
        if (!skybox.Initialize(pDevice, pContext)) {
            MessageBox(hWnd, L"Failed to initialize skybox", L"Error", MB_OK);
            return false;
        }

        if (!horizon.Initialize(pDevice, 3.0f, 3.0f)) {
            MessageBox(hWnd, L"Failed to initialize horizon plane", L"Error", MB_OK);
            return false;
        }

        HRESULT hr1 = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr1)) return false;

        hr1 = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(hr1)) {
            CoUninitialize();
            return false;
        }

        hr1 = pXAudio2->CreateMasteringVoice(&pMasterVoice);
        if (FAILED(hr1)) {
            SafeRelease(&pXAudio2);
            CoUninitialize();
            return false;
        }

        WAVEFORMATEX waveFormat = {};
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = 2;
        waveFormat.nSamplesPerSec = 44100;
        waveFormat.wBitsPerSample = 16;
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

        hr = pXAudio2->CreateSourceVoice(&pLandSound, &waveFormat);
        if (FAILED(hr)) {
            SafeRelease(&pMasterVoice);
            SafeRelease(&pXAudio2);
            CoUninitialize();
            return false;
        }

        XAUDIO2_BUFFER buffer = {};
        buffer.AudioBytes = sizeof(rawData) - 44;
        buffer.pAudioData = rawData + 44;
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.LoopCount = 0;

        hr = pLandSound->SubmitSourceBuffer(&buffer);
        if (FAILED(hr)) {
            SafeRelease(&pLandSound);
            SafeRelease(&pMasterVoice);
            SafeRelease(&pXAudio2);
            CoUninitialize();
            return false;
        }

        wasGrounded = false;

        worldMatrix = XMMatrixIdentity();
        viewMatrix = XMMatrixLookAtLH(
            XMVectorSet(0.f, 1.6f, -5.f, 0.f),
            XMVectorSet(0.f, 1.6f, 0.f, 0.f),
            XMVectorSet(0.f, 1.f, 0.f, 0.f));
        projectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (float)height, 0.01f, 500.f);

        if (!CreateShaders()) return false;
        if (!Create2DShaders()) return false;
        if (!CreateCheckerTextureSRV(64, 16, &pCubeSRV)) return false;
        if (!CreateGrassTextureSRV(64, &pGrassSRV)) return false;
        if (!CreateStoneTextureSRV(64, &pStoneSRV)) return false;
        if (!CreateWoodTextureSRV(64, &pWoodSRV)) return false;
        if (!CreateMetalTextureSRV(64, &pMetalSRV)) return false;
        if (!CreateBrickTextureSRV(64, &pBrickSRV)) return false;
        if (!CreateDirtTextureSRV(64, &pDirtSRV)) return false;
        if (!CreateWaterTextureSRV(2048, &pWaterSRV)) return false;
        if (!CreateLavaTextureSRV(2048, &pLavaSRV)) return false;
        if (!CreatePlayerTextureSRV(2048, &pPlayerSRV)) return false;
        if (!CreateWhiteTexture()) return false;
        if (!CreateInventoryMesh()) return false;
        if (!CreateCreativeInventoryMesh()) return false;
        if (!CreateCubeMesh()) return false;
        if (!CreateFloorMesh()) return false;
        if (!CreatePlayerMesh()) return false;
        if (!CreateCrosshairMesh()) return false;
        if (!CreateHealthBarMesh()) return false;

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.MaxAnisotropy = 16;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = pDevice->CreateSamplerState(&sampDesc, &pSampler);
        if (FAILED(hr)) return false;

        UpdateCamera();
        RegisterCommands();

        parkourObjects = ParkourMap::CreateParkourCourse();
        parkourBalls = ParkourMap::CreateParkourBalls();
        for (const auto& parkourObj : parkourObjects) {
            MapObject obj;
            obj.position = parkourObj.position;
            obj.rotation = parkourObj.rotation;
            obj.scale = parkourObj.scale;
            obj.type = parkourObj.type;
            mapObjects.push_back(obj);
        }
        for (const auto& ball : parkourBalls) {
            MapObject obj;
            obj.position = ball.position;
            obj.rotation = ball.rotation;
            obj.scale = ball.scale;
            obj.type = ball.type;
            mapObjects.push_back(obj);
        }

        return true;
}
void GameEngine::CreateCube(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices,
    float x, float y, float z, float width, float height, float depth)
{
    uint16_t baseIndex = (uint16_t)vertices.size();

    float halfW = width / 2;
    float halfH = height / 2;
    float halfD = depth / 2;

    Vertex v[] = {
        // Front
        {{x - halfW, y - halfH, z - halfD},{0,0,-1},{0,1}}, {{x - halfW, y + halfH, z - halfD},{0,0,-1},{0,0}},
        {{x + halfW, y + halfH, z - halfD},{0,0,-1},{1,0}}, {{x + halfW, y - halfH, z - halfD},{0,0,-1},{1,1}},
        // Back
        {{x - halfW, y - halfH, z + halfD},{0,0,1},{1,1}}, {{x + halfW, y - halfH, z + halfD},{0,0,1},{0,1}},
        {{x + halfW, y + halfH, z + halfD},{0,0,1},{0,0}}, {{x - halfW, y + halfH, z + halfD},{0,0,1},{1,0}},
        // Top
        {{x - halfW, y + halfH, z - halfD},{0,1,0},{0,1}}, {{x - halfW, y + halfH, z + halfD},{0,1,0},{0,0}},
        {{x + halfW, y + halfH, z + halfD},{0,1,0},{1,0}}, {{x + halfW, y + halfH, z - halfD},{0,1,0},{1,1}},
        // Bottom
        {{x - halfW, y - halfH, z - halfD},{0,-1,0},{0,0}}, {{x + halfW, y - halfH, z - halfD},{0,-1,0},{1,0}},
        {{x + halfW, y - halfH, z + halfD},{0,-1,0},{1,1}}, {{x - halfW, y - halfH, z + halfD},{0,-1,0},{0,1}},
        // Left
        {{x - halfW, y - halfH, z + halfD},{-1,0,0},{0,1}}, {{x - halfW, y + halfH, z + halfD},{-1,0,0},{0,0}},
        {{x - halfW, y + halfH, z - halfD},{-1,0,0},{1,0}}, {{x - halfW, y - halfH, z - halfD},{-1,0,0},{1,1}},
        // Right
        {{x + halfW, y - halfH, z - halfD},{1,0,0},{0,1}}, {{x + halfW, y + halfH, z - halfD},{1,0,0},{0,0}},
        {{x + halfW, y + halfH, z + halfD},{1,0,0},{1,0}}, {{x + halfW, y - halfH, z + halfD},{1,0,0},{1,1}},
    };

    uint16_t idx[] = {
        0,1,2,
        0,2,3,
        4,5,6,
        4,6,7,
        8,9,10,
        8,10,11,
        12,13,14,
        12,14,15,
        16,17,18,
        16,18,19,
        20,21,22,
        20,22,23
    };

    for (int i = 0; i < 24; i++) {
        vertices.push_back(v[i]);
    }
    for (int i = 0; i < 36; i++) {
        indices.push_back(baseIndex + idx[i]);
    }
}

bool GameEngine::CreateShaders()
{
    HRESULT hr = S_OK; ID3DBlob* err = nullptr;

    const char* vsSource = R"(
struct VS_INPUT { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
struct PS_INPUT { float4 position:SV_POSITION; float3 worldPos:TEXCOORD0; float3 normal:TEXCOORD1; float2 uv:TEXCOORD2; };
cbuffer ConstantBuffer : register(b0){
    matrix World; matrix View; matrix Projection;
    float3 CameraPos; float _pad0;
    float3 LightDir;  float _pad1;
    float3 LightColor;float _pad2;
    float3 Ambient;   float _pad3;
};
PS_INPUT main(VS_INPUT i){
    PS_INPUT o;
    float4 wp = mul(float4(i.position,1), World);
    o.worldPos = wp.xyz;
    o.normal = normalize(mul(float4(i.normal,0), World).xyz);
    o.uv = i.uv;
    o.position = mul(mul(wp, View), Projection);
    return o;
})";

    const char* psSource = R"(
Texture2D tex0:register(t0); SamplerState sam0:register(s0);
struct PS_INPUT { float4 position:SV_POSITION; float3 worldPos:TEXCOORD0; float3 normal:TEXCOORD1; float2 uv:TEXCOORD2; };
cbuffer ConstantBuffer : register(b0){
    matrix World; matrix View; matrix Projection;
    float3 CameraPos; float _pad0;
    float3 LightDir;  float _pad1;
    float3 LightColor;float _pad2;
    float3 Ambient;   float _pad3;
};
float4 main(PS_INPUT i):SV_Target{
    float3 N = normalize(i.normal);
    float3 L = normalize(-LightDir);
    float3 V = normalize(CameraPos - i.worldPos);
    float3 H = normalize(L+V);
    float diff = saturate(dot(N,L));
    float spec = pow(saturate(dot(N,H)), 512.0) * 0.1;
    float3 albedo = tex0.Sample(sam0, i.uv).rgb;
    float3 color = Ambient * albedo + (albedo*diff + spec) * LightColor;
    return float4(color,1);
})";

    ID3DBlob* vs = nullptr; ID3DBlob* ps = nullptr;
    hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vs, &err);
    if (FAILED(hr)) { if (err) OutputDebugStringA((char*)err->GetBufferPointer()); SafeRelease(&err); return false; }
    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &ps, &err);
    if (FAILED(hr)) { if (err) OutputDebugStringA((char*)err->GetBufferPointer()); SafeRelease(&err); SafeRelease(&vs); return false; }

    hr = pDevice->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &pVS);
    if (FAILED(hr)) { SafeRelease(&vs); SafeRelease(&ps); return false; }
    hr = pDevice->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pPS);
    if (FAILED(hr)) { SafeRelease(&vs); SafeRelease(&ps); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,  D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",  0,DXGI_FORMAT_R32G32B32_FLOAT,0,12, D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,   0,24, D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    hr = pDevice->CreateInputLayout(layout, 3, vs->GetBufferPointer(), vs->GetBufferSize(), &pInputLayout);
    SafeRelease(&vs); SafeRelease(&ps);
    if (FAILED(hr)) return false;
    pContext->IASetInputLayout(pInputLayout);

    // constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = pDevice->CreateBuffer(&cbd, nullptr, &pCB);
    if (FAILED(hr)) return false;

    // sampler
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.MaxAnisotropy = 8;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0; sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = pDevice->CreateSamplerState(&sd, &pSampler);
    return SUCCEEDED(hr);
}


bool GameEngine::CreateCubeMesh()
{
    Vertex v[] = {
        // Front
        {{-1,-1,-1},{0,0,-1},{0,1}}, {{-1, 1,-1},{0,0,-1},{0,0}}, {{ 1, 1,-1},{0,0,-1},{1,0}}, {{ 1,-1,-1},{0,0,-1},{1,1}},
        // Back
        {{-1,-1, 1},{0,0, 1},{1,1}}, {{ 1,-1, 1},{0,0, 1},{0,1}}, {{ 1, 1, 1},{0,0, 1},{0,0}}, {{-1, 1, 1},{0,0, 1},{1,0}},
        // Top
        {{-1, 1,-1},{0,1,0},{0,1}}, {{-1, 1, 1},{0,1,0},{0,0}}, {{ 1, 1, 1},{0,1,0},{1,0}}, {{ 1, 1,-1},{0,1,0},{1,1}},
        // Bottom
        {{-1,-1,-1},{0,-1,0},{0,0}}, {{ 1,-1,-1},{0,-1,0},{1,0}}, {{ 1,-1, 1},{0,-1,0},{1,1}}, {{-1,-1, 1},{0,-1,0},{0,1}},
        // Left
        {{-1,-1, 1},{-1,0,0},{0,1}}, {{-1, 1, 1},{-1,0,0},{0,0}}, {{-1, 1,-1},{-1,0,0},{1,0}}, {{-1,-1,-1},{-1,0,0},{1,1}},
        // Right
        {{ 1,-1,-1},{ 1,0,0},{0,1}}, {{ 1, 1,-1},{ 1,0,0},{0,0}}, {{ 1, 1, 1},{ 1,0,0},{1,0}}, {{ 1,-1, 1},{ 1,0,0},{1,1}},
    };
    uint16_t idx[] = {
        0,1,2, 0,2,3,  4,5,6, 4,6,7,  8,9,10, 8,10,11,
        12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23
    };
    cubeIndexCount = (UINT)_countof(idx);

    D3D11_BUFFER_DESC vb{ sizeof(v), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA vbd{ v };
    if (FAILED(pDevice->CreateBuffer(&vb, &vbd, &pCubeVB))) return false;

    D3D11_BUFFER_DESC ib{ sizeof(idx), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER };
    D3D11_SUBRESOURCE_DATA ibd{ idx };
    if (FAILED(pDevice->CreateBuffer(&ib, &ibd, &pCubeIB))) return false;

    return true;
}

bool GameEngine::CreateFloorMesh()
{
    const float S = 450.f;
    const float T = 450.f;
    Vertex v[] = {
        {{-S,0,-S},{0,1,0},{0, T}},
        {{-S,0, S},{0,1,0},{0, 0}},
        {{ S,0, S},{0,1,0},{T, 0}},
        {{ S,0,-S},{0,1,0},{T, T}},
    };
    uint16_t idx[] = { 0,1,2, 0,2,3 };
    floorIndexCount = (UINT)_countof(idx);

    D3D11_BUFFER_DESC vb{ sizeof(v), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA vbd{ v };
    if (FAILED(pDevice->CreateBuffer(&vb, &vbd, &pFloorVB))) return false;

    D3D11_BUFFER_DESC ib{ sizeof(idx), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER };
    D3D11_SUBRESOURCE_DATA ibd{ idx };
    if (FAILED(pDevice->CreateBuffer(&ib, &ibd, &pFloorIB))) return false;

    return true;
}

float GetTimeSeconds()
{
    static LARGE_INTEGER freq, start;
    static bool initialized = false;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        initialized = true;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return float(now.QuadPart - start.QuadPart) / freq.QuadPart;
}

void GameEngine::Render()
{
    if (!pDevice || !pContext || !pSwapChain) {
        return;
    }

    // Skybox 
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    pContext->ClearRenderTargetView(pRTV, clearColor);
    pContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    ID3D11RasterizerState* oldRasterState = nullptr;
    pContext->RSGetState(&oldRasterState);

    skybox.Render(pContext, viewMatrix, projectionMatrix);

    // Clouds
    float time = GetTimeSeconds();
    //clouds.Render(pContext, viewMatrix, projectionMatrix, time);

    // --- Fog ---
    horizon.Render(pContext, viewMatrix, projectionMatrix, cameraPosition, time);

    if (oldRasterState) {
        pContext->RSSetState(oldRasterState);
        oldRasterState->Release();
    }

    // 3D render
    UINT stride3D = sizeof(Vertex);
    UINT offset3D = 0;

    pContext->IASetInputLayout(pInputLayout);
    pContext->VSSetShader(pVS, nullptr, 0);
    pContext->PSSetShader(pPS, nullptr, 0);
    pContext->PSSetSamplers(0, 1, &pSampler);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_MAPPED_SUBRESOURCE mapped3D{};

    // Floor
    pContext->IASetVertexBuffers(0, 1, &pFloorVB, &stride3D, &offset3D);
    pContext->IASetIndexBuffer(pFloorIB, DXGI_FORMAT_R16_UINT, 0);

    pContext->Map(pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped3D);
    auto* cb3D = reinterpret_cast<ConstantBuffer*>(mapped3D.pData);
    cb3D->world = XMMatrixTranspose(XMMatrixIdentity());
    cb3D->view = XMMatrixTranspose(viewMatrix);
    cb3D->projection = XMMatrixTranspose(projectionMatrix);
    cb3D->cameraPos = cameraPosition;

    XMFLOAT3 lightDir, lightColor, ambient;
    GetCurrentLighting(lightDir, lightColor, ambient);
    cb3D->lightDir = lightDir;
    cb3D->lightColor = lightColor;
    cb3D->ambient = ambient;

    pContext->Unmap(pCB, 0);

    pContext->VSSetConstantBuffers(0, 1, &pCB);
    pContext->PSSetConstantBuffers(0, 1, &pCB);
    pContext->PSSetShaderResources(0, 1, &pGrassSRV);
    pContext->DrawIndexed(floorIndexCount, 0, 0);

    // Render map obj
    pContext->IASetVertexBuffers(0, 1, &pCubeVB, &stride3D, &offset3D);
    pContext->IASetIndexBuffer(pCubeIB, DXGI_FORMAT_R16_UINT, 0);

    for (auto& obj : mapObjects) {
        XMMATRIX world = XMMatrixScaling(obj.scale.x, obj.scale.y, obj.scale.z) *
            XMMatrixRotationRollPitchYaw(obj.rotation.x, obj.rotation.y, obj.rotation.z) *
            XMMatrixTranslation(obj.position.x, obj.position.y, obj.position.z);

        ID3D11ShaderResourceView* texture = nullptr;
        if (obj.type == "grass") texture = pGrassSRV;
        else if (obj.type == "stone") texture = pStoneSRV;
        else if (obj.type == "wood") texture = pWoodSRV;
        else if (obj.type == "metal") texture = pMetalSRV;
        else if (obj.type == "brick") texture = pBrickSRV;
        else if (obj.type == "dirt") texture = pDirtSRV;
        else if (obj.type == "water") texture = pWaterSRV;
        else if (obj.type == "lava") texture = pLavaSRV;
        else texture = pCubeSRV;

        pContext->Map(pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped3D);
        cb3D = reinterpret_cast<ConstantBuffer*>(mapped3D.pData);
        cb3D->world = XMMatrixTranspose(world);
        cb3D->view = XMMatrixTranspose(viewMatrix);
        cb3D->projection = XMMatrixTranspose(projectionMatrix);
        cb3D->cameraPos = cameraPosition;
        cb3D->lightDir = lightDir;
        cb3D->lightColor = lightColor;
        cb3D->ambient = ambient;
        pContext->Unmap(pCB, 0);

        pContext->PSSetShaderResources(0, 1, &texture);
        pContext->DrawIndexed(cubeIndexCount, 0, 0);
    }

    // Render Player
    if (thirdPerson) {
        RenderPlayer(playerPos, cameraRotation.y, true, cameraRotation.x);
    }
    else { // firstperson
        RenderPlayer(playerPos, cameraRotation.y, true, cameraRotation.x);
    }

    if (inventoryOpen) {
        RenderCreativeInventory();
    }

    // render network player
    if (isMultiplayer && !networkPlayers.empty()) {
        for (const auto& pair : networkPlayers) {
            const NetworkPlayer& player = pair.second;
            int currentId = networkManager.IsServer() ? -1 : 0;
            if (pair.first == currentId && !thirdPerson /*|| thirdPerson*/) continue;

            RenderPlayer(player.position, 0.0f, false);
        }
    }

    // 2d render
    RECT rc;
    GetClientRect(hWnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);
    float height = static_cast<float>(rc.bottom - rc.top);
    XMMATRIX orthoMatrix = XMMatrixOrthographicLH(width, height, 0.0f, 1.0f);

    pContext->IASetInputLayout(p2DInputLayout);
    pContext->VSSetShader(p2DVS, nullptr, 0);
    pContext->PSSetShader(p2DPS, nullptr, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_MAPPED_SUBRESOURCE mapped2D{};
    XMMATRIX* cb2D = nullptr;

    // Inventory
    pContext->Map(p2DCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped2D);
    cb2D = reinterpret_cast<XMMATRIX*>(mapped2D.pData);
    *cb2D = XMMatrixTranspose(orthoMatrix);
    pContext->Unmap(p2DCB, 0);
    pContext->VSSetConstantBuffers(0, 1, &p2DCB);

    UINT stride2D = sizeof(XMFLOAT2) + sizeof(XMFLOAT4) + sizeof(XMFLOAT2);
    UINT offset2D = 0;
    pContext->IASetVertexBuffers(0, 1, &pInventoryVB, &stride2D, &offset2D);
    pContext->IASetIndexBuffer(pInventoryIB, DXGI_FORMAT_R16_UINT, 0);

    if (gameMode == BUILD_MODE) {
        pContext->IASetInputLayout(p2DInputLayout);
        pContext->VSSetShader(p2DVS, nullptr, 0);
        pContext->PSSetShader(p2DPS, nullptr, 0);

        pContext->Map(p2DCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped2D);
        cb2D = reinterpret_cast<XMMATRIX*>(mapped2D.pData);
        *cb2D = XMMatrixTranspose(orthoMatrix);
        pContext->Unmap(p2DCB, 0);
        pContext->VSSetConstantBuffers(0, 1, &p2DCB);

        UINT stride2D = sizeof(XMFLOAT2) + sizeof(XMFLOAT4) + sizeof(XMFLOAT2);
        UINT offset2D = 0;

        //render slots
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pContext->IASetVertexBuffers(0, 1, &pInventoryVB, &stride2D, &offset2D);
        pContext->IASetIndexBuffer(pInventoryIB, DXGI_FORMAT_R16_UINT, 0);

        for (int i = 0; i < INVENTORY_SLOTS; ++i)
        {
            ID3D11ShaderResourceView* texture = nullptr;
            if (inventoryBlocks[i] == "grass") texture = pGrassSRV;
            else if (inventoryBlocks[i] == "stone") texture = pStoneSRV;
            else if (inventoryBlocks[i] == "wood") texture = pWoodSRV;
            else if (inventoryBlocks[i] == "metal") texture = pMetalSRV;
            else if (inventoryBlocks[i] == "brick") texture = pBrickSRV;
            else if (inventoryBlocks[i] == "dirt") texture = pDirtSRV;
            else if (inventoryBlocks[i] == "water") texture = pWaterSRV;
            else if (inventoryBlocks[i] == "lava") texture = pLavaSRV;
            else texture = pCubeSRV;

            pContext->PSSetShaderResources(0, 1, &texture);
            pContext->PSSetSamplers(0, 1, &pSampler);
            pContext->DrawIndexed(6, i * 6, 0);
        }

        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        pContext->IASetVertexBuffers(0, 1, &pInventorySelectVB, &stride2D, &offset2D);
        pContext->IASetIndexBuffer(pInventorySelectIB, DXGI_FORMAT_R16_UINT, 0);
        pContext->PSSetShaderResources(0, 1, &pWhiteTextureSRV);
        pContext->DrawIndexed(inventorySelectIndexCount, 0, 0);

        // ret topology back
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    if (!thirdPerson || thirdPerson) { 
        RECT rc;
        GetClientRect(hWnd, &rc);
        float width = static_cast<float>(rc.right - rc.left);
        float height = static_cast<float>(rc.bottom - rc.top);
        XMMATRIX orthoMatrix = XMMatrixOrthographicLH(width, height, 0.0f, 1.0f);

        XMMATRIX crosshairTransform = XMMatrixTranslation(0.0f, 0.0f, 0.0f);

        D3D11_MAPPED_SUBRESOURCE mapped2D{};
        pContext->Map(p2DCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped2D);
        XMMATRIX* cb2D = reinterpret_cast<XMMATRIX*>(mapped2D.pData);
        *cb2D = XMMatrixTranspose(crosshairTransform * orthoMatrix);
        pContext->Unmap(p2DCB, 0);

        pContext->VSSetConstantBuffers(0, 1, &p2DCB);

        UINT stride2D = sizeof(XMFLOAT2) + sizeof(XMFLOAT4);
        UINT offset2D = 0;
        pContext->IASetVertexBuffers(0, 1, &pCrosshairVB, &stride2D, &offset2D);
        pContext->IASetIndexBuffer(pCrosshairIB, DXGI_FORMAT_R16_UINT, 0);
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        pContext->PSSetShaderResources(0, 1, &pWhiteTextureSRV);
        pContext->PSSetSamplers(0, 1, &pSampler);
        pContext->DrawIndexed(crosshairIndexCount, 0, 0);

        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    //UpdateHealthBar(health / 100.0f); // Convert to 0.0-1.0 scale

    XMMATRIX healthTransform = XMMatrixTranslation(-width / 2 + 100.0f, height / 2 - 50.0f, 0.0f) *
        XMLoadFloat4x4(&healthBarWorldMatrix);

    pContext->Map(p2DCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped2D);
    cb2D = reinterpret_cast<XMMATRIX*>(mapped2D.pData);
    *cb2D = XMMatrixTranspose(healthTransform * orthoMatrix);
    pContext->Unmap(p2DCB, 0);

    pContext->IASetVertexBuffers(0, 1, &pHealthVB, &stride2D, &offset2D);
    pContext->IASetIndexBuffer(pHealthIB, DXGI_FORMAT_R16_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->PSSetShaderResources(0, 1, &pWhiteTextureSRV); 
    pContext->DrawIndexed(healthIndexCount, 0, 0);

    // polovoi kadr
    pSwapChain->Present(1, 0);
    if (!consoleActive) {
        std::string inventoryInfo = "Inventory: ";
        if (gameMode == BUILD_MODE) {
            for (int i = 0; i < INVENTORY_SLOTS; i++) {
                if (i < inventoryBlocks.size()) 
                { 
                    if (i == selectedInventorySlot) {
                        inventoryInfo += "[" + inventoryBlocks[i] + "] ";
                    }
                    else {
                        inventoryInfo += inventoryBlocks[i] + " ";
                    }
                }
                else {
                    if (i == selectedInventorySlot) {
                        inventoryInfo += "[empty] "; 
                    }
                    else {
                        inventoryInfo += "_ "; 
                    }
                }
            }
        }
        else {
            inventoryInfo += "N/A (HVH Mode)";
        }
    }
}


void GameEngine::Cleanup()
{
    // network
    networkManager.Disconnect();

    // audio
    if (pLandSound) {
        pLandSound->Stop();
        pLandSound->FlushSourceBuffers();
        SafeRelease(&pLandSound);
    }
    SafeRelease(&pMasterVoice);
    SafeRelease(&pXAudio2);

    // resource
    skybox.Cleanup();
    horizon.Cleanup();

    // DirectX 
    SafeRelease(&pRasterState);
    SafeRelease(&pPlayerSRV);
    SafeRelease(&pPlayerVB);
    SafeRelease(&pPlayerIB);
    SafeRelease(&pCubeSRV);
    SafeRelease(&pGrassSRV);
    SafeRelease(&pStoneSRV);
    SafeRelease(&pWoodSRV);
    SafeRelease(&pMetalSRV);
    SafeRelease(&pBrickSRV);
    SafeRelease(&pDirtSRV);
    SafeRelease(&pWaterSRV);
    SafeRelease(&pLavaSRV);
    SafeRelease(&pSampler);
    SafeRelease(&pCB);
    SafeRelease(&pInputLayout);
    SafeRelease(&pPS);
    SafeRelease(&pVS);
    SafeRelease(&p2DVS);
    SafeRelease(&p2DPS);
    SafeRelease(&p2DInputLayout);
    SafeRelease(&pCrosshairVB);
    SafeRelease(&pCrosshairIB);
    SafeRelease(&pInventoryVB);
    SafeRelease(&pInventoryIB);
    SafeRelease(&pInventorySelectVB);
    SafeRelease(&pInventorySelectIB);
    SafeRelease(&pCreativeInventoryVB);
    SafeRelease(&pCreativeInventoryIB);
    SafeRelease(&pCreativeSelectVB);
    SafeRelease(&pCreativeSelectIB);
    SafeRelease(&pWhiteTextureSRV);
    SafeRelease(&p2DCB);
    SafeRelease(&pFloorVB);
    SafeRelease(&pFloorIB);
    SafeRelease(&pCubeVB);
    SafeRelease(&pCubeIB);
    SafeRelease(&pHealthVB);
    SafeRelease(&pHealthIB);
    SafeRelease(&pDSV);
    SafeRelease(&pRTV);
    SafeRelease(&pSwapChain);
    SafeRelease(&pContext);
    SafeRelease(&pDevice);

    mapObjects.clear();
    networkPlayers.clear();

    // XAudio2
    CoUninitialize();
}

bool GameEngine::RayIntersectsAABB(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, const AABB& box, float& tNear) {
    float tMin = -INFINITY;
    float tMax = INFINITY;

    for (int i = 0; i < 3; ++i) {
        float r = (i == 0) ? rayDir.x : (i == 1) ? rayDir.y : rayDir.z;
        float o = (i == 0) ? rayOrigin.x : (i == 1) ? rayOrigin.y : rayOrigin.z;
        float bmin = (i == 0) ? box.min.x : (i == 1) ? box.min.y : box.min.z;
        float bmax = (i == 0) ? box.max.x : (i == 1) ? box.max.y : box.max.z;

        if (std::abs(r) < 0.0001f) {
            if (o < bmin || o > bmax) return false;
        }
        else {
            float ood = 1.0f / r;
            float t1 = (bmin - o) * ood;
            float t2 = (bmax - o) * ood;
            if (t1 > t2) std::swap(t1, t2);
#undef max
            tMin = std::max(tMin, t1);
#undef min
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }

    tNear = (tMin > 0.0f) ? tMin : tMax;
    return tNear > 0.0f && tNear < buildReach;
}

XMFLOAT3 GameEngine::GetPlacementPosition(const XMFLOAT3& hitPoint, const XMFLOAT3& normal) {
    XMFLOAT3 pos = hitPoint;
    pos.x += normal.x;
    pos.y += normal.y;
    pos.z += normal.z;
    pos.x = floorf(pos.x + 0.5f);
    pos.y = floorf(pos.y + 0.5f);
    pos.z = floorf(pos.z + 0.5f);
    return pos;
}

void GameEngine::HandleBuilding()
{
    if (gameMode == HVH_MODE) {
        return;
    }

    bool doBreaking = mouseStates[0];
    bool doPlacing = mouseStates[2];
    if (!doBreaking && !doPlacing) return;

    XMVECTOR defaultForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
    XMVECTOR lookDir = XMVector3TransformCoord(defaultForward, R);
    XMFLOAT3 rayDir;
    XMStoreFloat3(&rayDir, XMVector3Normalize(lookDir));
    XMFLOAT3 rayOrigin = cameraPosition;

    float playerHeight = 1.8f;
    float playerHalfWidth = 0.3f;
    float playerEyeHeight = 1.6f;

    if (doPlacing) {
        bool hitFloor = false;
        float tFloor = -rayOrigin.y / rayDir.y;
        if (tFloor > 0.0f && tFloor < buildReach && rayDir.y < 0.0f) {
            hitFloor = true;
            XMFLOAT3 hitPointFloor;
            hitPointFloor.x = rayOrigin.x + tFloor * rayDir.x;
            hitPointFloor.y = 0.0f;
            hitPointFloor.z = rayOrigin.z + tFloor * rayDir.z;
            XMFLOAT3 newPos = GetPlacementPosition(hitPointFloor, XMFLOAT3(0.f, 1.f, 0.f));
            newPos.y = 1.0f;

            if (!IsBlockAtPosition(newPos)) {
                AABB newBlockAABB;
                newBlockAABB.min = { newPos.x - 1.0f, newPos.y - 1.0f, newPos.z - 1.0f };
                newBlockAABB.max = { newPos.x + 1.0f, newPos.y + 1.0f, newPos.z + 1.0f };

                AABB playerAABB;
                playerAABB.min = { playerPos.x - playerHalfWidth, playerPos.y, playerPos.z - playerHalfWidth };
                playerAABB.max = { playerPos.x + playerHalfWidth, playerPos.y + playerHeight, playerPos.z + playerHalfWidth };

                bool intersectsPlayer = (playerAABB.min.x <= newBlockAABB.max.x && playerAABB.max.x >= newBlockAABB.min.x &&
                    playerAABB.min.y <= newBlockAABB.max.y && playerAABB.max.y >= newBlockAABB.min.y &&
                    playerAABB.min.z <= newBlockAABB.max.z && playerAABB.max.z >= newBlockAABB.min.z);

                if (!intersectsPlayer || newPos.y < playerPos.y + 0.001f) {
                    MapObject obj;
                    obj.position = newPos;
                    obj.rotation = { 0.f, 0.f, 0.f };
                    obj.scale = { 1.f, 1.f, 1.f };
                    obj.type = inventoryBlocks[selectedInventorySlot];
                    mapObjects.push_back(obj);

                    /*AddToHistory("Block placed at " +
                        std::to_string((int)newPos.x) + "," +
                        std::to_string((int)newPos.y) + "," +
                        std::to_string((int)newPos.z));*/

                    if (isMultiplayer && isSynchronized) {
                        std::string blockData = "BLOCK:ADD|" +
                            std::to_string(newPos.x) + "," +
                            std::to_string(newPos.y) + "," +
                            std::to_string(newPos.z) + "|" +
                            inventoryBlocks[selectedInventorySlot];
                        if (networkManager.IsServer()) {
                            networkManager.BroadcastData(blockData);
                            AddToHistory("Block placement broadcasted to all clients");
                        }
                        else {
                            networkManager.SendData(blockData);
                            AddToHistory("Block placement sent to server");
                        }
                    }

                    if (fabs(newPos.x - playerPos.x) < playerHalfWidth &&
                        fabs(newPos.z - playerPos.z) < playerHalfWidth &&
                        newPos.y > playerPos.y - 0.001f && newPos.y < playerPos.y + playerHeight) {
                        playerPos.y = newPos.y + 1.0f + 0.001f;
                        velocity.y = 0.0f;
                        isGrounded = true;
                    }
                }
            }
            mouseStates[2] = false;
            return;
        }
    }

    float closestT = buildReach;
    int hitIndex = -1;
    XMFLOAT3 hitNormal = { 0.f, 0.f, 0.f };
    XMFLOAT3 hitPoint = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < (int)mapObjects.size(); ++i) {
        const auto& obj = mapObjects[i];
        AABB box;
        box.min = { obj.position.x - obj.scale.x, obj.position.y - obj.scale.y, obj.position.z - obj.scale.z };
        box.max = { obj.position.x + obj.scale.x, obj.position.y + obj.scale.y, obj.position.z + obj.scale.z };

        float t;
        if (RayIntersectsAABB(rayOrigin, rayDir, box, t) && t < closestT) {
            closestT = t;
            hitIndex = i;
            hitPoint.x = rayOrigin.x + t * rayDir.x;
            hitPoint.y = rayOrigin.y + t * rayDir.y;
            hitPoint.z = rayOrigin.z + t * rayDir.z;

            float eps = 0.01f;
            if (abs(hitPoint.x - box.min.x) < eps) hitNormal = { -1.f, 0.f, 0.f };
            else if (abs(hitPoint.x - box.max.x) < eps) hitNormal = { 1.f, 0.f, 0.f };
            else if (abs(hitPoint.y - box.min.y) < eps) hitNormal = { 0.f, -1.f, 0.f };
            else if (abs(hitPoint.y - box.max.y) < eps) hitNormal = { 0.f, 1.f, 0.f };
            else if (abs(hitPoint.z - box.min.z) < eps) hitNormal = { 0.f, 0.f, -1.f };
            else if (abs(hitPoint.z - box.max.z) < eps) hitNormal = { 0.f, 0.f, 1.f };
        }
    }

    if (hitIndex != -1) {
        if (doBreaking) {
            const auto& obj = mapObjects[hitIndex];

            if (isMultiplayer) {
                std::string removeData = "BLOCK:REMOVE|" +
                    std::to_string(obj.position.x) + "," +
                    std::to_string(obj.position.y) + "," +
                    std::to_string(obj.position.z);

                if (networkManager.IsServer()) {
                    networkManager.BroadcastData(removeData);
                    AddToHistory("Block removal broadcasted to all clients");
                }
                else {
                    networkManager.SendData(removeData);
                    AddToHistory("Block removal sent to server");
                }
            }

            mapObjects.erase(mapObjects.begin() + hitIndex);
            mouseStates[0] = false;
        }
        else if (doPlacing) {
            XMFLOAT3 newPos = GetPlacementPosition(hitPoint, hitNormal);

            if (!IsBlockAtPosition(newPos)) {
                AABB newBlockAABB;
                newBlockAABB.min = { newPos.x - 0.5f, newPos.y - 0.5f, newPos.z - 0.5f };
                newBlockAABB.max = { newPos.x + 0.5f, newPos.y + 0.5f, newPos.z + 0.5f };

                AABB playerAABB;
                playerAABB.min = { playerPos.x - playerHalfWidth, playerPos.y, playerPos.z - playerHalfWidth };
                playerAABB.max = { playerPos.x + playerHalfWidth, playerPos.y + playerHeight, playerPos.z + playerHalfWidth };

                bool intersectsPlayer = (playerAABB.min.x <= newBlockAABB.max.x && playerAABB.max.x >= newBlockAABB.min.x &&
                    playerAABB.min.y <= newBlockAABB.max.y && playerAABB.max.y >= newBlockAABB.min.y &&
                    playerAABB.min.z <= newBlockAABB.max.z && playerAABB.max.z >= newBlockAABB.min.z);

                if (!intersectsPlayer) {
                    MapObject obj;
                    obj.position = newPos;
                    obj.rotation = { 0.f, 0.f, 0.f };
                    obj.scale = { 1.f, 1.f, 1.f };
                    obj.type = inventoryBlocks[selectedInventorySlot];
                    mapObjects.push_back(obj);

                    if (isMultiplayer) {
                        std::string blockData = "BLOCK:ADD|" +
                            std::to_string(newPos.x) + "," +
                            std::to_string(newPos.y) + "," +
                            std::to_string(newPos.z) + "|" +
                            inventoryBlocks[selectedInventorySlot];

                        if (networkManager.IsServer()) {
                            networkManager.BroadcastData(blockData);
                            AddToHistory("Block placement broadcasted to all clients");
                        }
                        else {
                            networkManager.SendData(blockData);
                            AddToHistory("Block placement sent to server");
                        }
                    }
                }
            }
            mouseStates[2] = false;
        }
    }
}

bool GameEngine::CreateTextureFromData(const BYTE* data, UINT width, UINT height, ID3D11ShaderResourceView** outSRV)
{
    HRESULT hr;
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* pTexture = nullptr;
    hr = pDevice->CreateTexture2D(&texDesc, nullptr, &pTexture);
    if (FAILED(hr)) return false;

    pContext->UpdateSubresource(pTexture, 0, nullptr, data, width * 4, 0);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;
    hr = pDevice->CreateShaderResourceView(pTexture, &srvDesc, outSRV);
    if (FAILED(hr)) { SafeRelease(&pTexture); return false; }

    pContext->GenerateMips(*outSRV);
    SafeRelease(&pTexture);
    return true;
}


