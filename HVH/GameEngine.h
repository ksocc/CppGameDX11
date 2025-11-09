#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h> 
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include <mutex>
#include <array>
#include <string>
#include <functional>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include "map1.h"
#include "Jmp.h"
#include <map>
#include "NetworkManager.h"
#include <d2d1.h>
#include <dwrite.h> 
#include "SafeRelease.h"
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <Audiopolicy.h>
#include <mmreg.h>
#include "Gamemod.h"

#pragma comment(lib, "winmm.lib")

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib") 

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "Skybox.h" 

using namespace DirectX;


class GameEngine
{
public:
    explicit GameEngine(HWND window);
    ~GameEngine();

    Skybox skybox;
    HorizonPlane horizon;

    XMFLOAT3 GetCameraPosition() const { return cameraPosition; }

    template<typename T>
    T clamp(T value, T minVal, T maxVal)
    {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }

    bool Initialize();
    void Update(float dt);
    void Render();
    void OnResize(UINT width, UINT height);
    void OnKeyDown(UINT key);
    void OnKeyUp(UINT key);
    void HandleInventoryMouse(int x, int y);
    void OnMouseWheel(short delta);
    void OnChar(UINT ch);
    void OnMouseMove(int x, int y, bool captureMouse);
    void Cleanup();
    void Physics(float dt);

    //players
    bool CreatePlayerTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreatePlayerMesh();
    //players

    void ApplyLighting(const XMFLOAT3& lightDir, const XMFLOAT3& lightColor, const XMFLOAT3& ambient) {
        currentLightDir = lightDir;
        currentLightColor = lightColor;
        currentAmbient = ambient;
    };

    void GetCurrentLighting(XMFLOAT3& lightDir, XMFLOAT3& lightColor, XMFLOAT3& ambient) const {
        lightDir = currentLightDir;
        lightColor = currentLightColor;
        ambient = currentAmbient;
    };

    //methods

    //inventory
    bool CreateInventoryMesh();
    void UpdateInventorySelection();
    void UpdateCreativeInventorySelection();
    bool IsKeySpecial(UINT key);
    void OpenInventory();
    void CloseInventory();
    ID3D11ShaderResourceView* GetTextureForBlock(const std::string& blockType);
    void RenderBlockInInventorySlot(int slotIndex, const std::string& blockType);
    void RenderCreativeInventory();
    bool CreateCreativeInventoryMesh();
    bool CreateWhiteTexture();
    //inventory

    //---base
    bool CreateTextureFromData(const BYTE* data, UINT width, UINT height, ID3D11ShaderResourceView** outSRV);
    bool CreateCheckerTextureSRV(UINT size, UINT tiles, ID3D11ShaderResourceView** outSRV);
    bool CreateGrassTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateMetalTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateStoneTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateWoodTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateBrickTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateDirtTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateWaterTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    bool CreateLavaTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV);
    //---base

    //crosshair
    bool CreateCrosshairMesh();
    bool CreateHealthBarMesh();
    void UpdateHealthBar(float healthPercentage);
    //crosshair

    void SetGameMode(GameMode mode);
    GameMode GetGameMode() const;

    float healthBarYOffset;
    float healthBarXOffset;
    float healthBarWidth;
    float healthBarHeight;

    struct PlayerPart {
        std::string name;
        XMFLOAT3 size;
        XMFLOAT3 pivot;
        uint32_t indexCount;
        uint32_t indexStart;
    };

    void RenderPlayer(const XMFLOAT3& pos, float rotationY, bool isSelf, float headPitch = 0.0f);

    std::vector<PlayerPart> playerParts;

    float walkTime = 0.0f;
    XMFLOAT3 playerVelocity = { 0,0,0 };


    int GetHealth() const { return health; }
    void SetHealth(int newHealth) { health = clamp(newHealth, 0, 100); }

    void OnMouseDown(UINT button);
    void HandleInventoryMouseClick(UINT button);
    void HandleBuildModeMouseClick(UINT button);
    void HandleHvHModeMouseClick(UINT button);
    void PlaceBlock(const XMFLOAT3& position);
    void OnMouseUp(UINT button);

    void SetJumpVolume(float volume);
    float GetJumpVolume() const;

    /// console
    bool IsConsoleActive() const {
        return consoleActive;
    };

    void ToggleConsole() {
        consoleActive = !consoleActive;
        if (consoleActive) consoleInput.clear();
    };

    void SetConsoleInput(const std::string& input)
    {
        consoleInput = input;
    };

    void CycleConsoleHistory(bool up);

    const std::vector<std::string>& GetConsoleHistory() const {
        return consoleHistory;
    };

    const std::string& GetConsoleInput() const {
        return consoleInput;
    };

    std::string currentTabCompletionPrefix;
    std::vector<std::string> currentTabMatches;
    const std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>>& GetCommands() const {
        return commands;
    };

    size_t currentTabCompletionIndex = 0;
    /// console

    UINT playerIndexCount;
    bool thirdPerson;
    float thirdPersonDistance;

    struct NetworkPlayer {
        XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
        std::string name = "";
        int clientId = 0;
    };

    struct NetworkBlock {
        XMFLOAT3 position;
        std::string type;
        bool toRemove;

        NetworkBlock()
            : position(0.0f, 0.0f, 0.0f)
            , type("")
            , toRemove(false)
        {
        }
    };

    struct MapObject {
        XMFLOAT3 position;
        XMFLOAT3 rotation;
        XMFLOAT3 scale;
        std::string type;
        ID3D11ShaderResourceView* texture;

        MapObject()
            : position(0.0f, 0.0f, 0.0f)
            , rotation(0.0f, 0.0f, 0.0f)
            , scale(1.0f, 1.0f, 1.0f)
            , type("")
            , texture(nullptr)
        {
        }
    };


private:
    NetworkManager networkManager;
    bool isMultiplayer = false;
    std::unordered_map<int, XMFLOAT3> otherPlayers; // ID -> pos

    std::unordered_map<int, NetworkPlayer> networkPlayers;
    std::vector<NetworkBlock> pendingBlocks; // blocs on serv
    float networkUpdateTimer = 0.0f;
    std::vector<std::string> consoleHistory;
    std::recursive_mutex consoleHistoryMutex;
    const size_t maxConsoleHistorySize = 100;
    int nextClientId = 1;

    float blockUpdateTimer;
    bool isSynchronized = false;
    float playerUpdateTimer;

    XMFLOAT3 currentLightDir;
    XMFLOAT3 currentLightColor;
    XMFLOAT3 currentAmbient;


    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
    };

    struct Vertex2D {
        DirectX::XMFLOAT2 position;
        DirectX::XMFLOAT4 color;
    };


    struct ConstantBuffer
    {
        DirectX::XMMATRIX world;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;
        DirectX::XMFLOAT3 cameraPos; float _pad0 = 0.f;
        DirectX::XMFLOAT3 lightDir;  float _pad1 = 0.f;
        DirectX::XMFLOAT3 lightColor;float _pad2 = 0.f;
        DirectX::XMFLOAT3 ambient;   float _pad3 = 0.f;
    };

    struct AABB {
        XMFLOAT3 min;
        XMFLOAT3 max;
    };

    std::vector<std::string> inventoryBlocks = {
    "grass", "stone", "wood", "metal", "brick", "dirt", "water", "lava", "cube"
    };
    int selectedInventorySlot = 0;
    const int INVENTORY_SLOTS = 9;
    bool inventoryOpen = false;
    std::vector<std::string> creativeBlocks = {
        "grass", "stone", "wood", "metal", "brick", "dirt", "water", "lava", "cube"
    };
    int creativeSelectedSlot = 0;
    const int CREATIVE_SLOTS_PER_PAGE = 36; 
    int creativeCurrentPage = 0;

    bool isDragging = false;
    int dragSourceSlot = -1;
    int dragTargetSlot = -1;
    std::string draggedBlockType = "";

    ID3D11Buffer* pCreativeInventoryVB = nullptr;
    ID3D11Buffer* pCreativeInventoryIB = nullptr;
    ID3D11Buffer* pCreativeSelectVB = nullptr;
    ID3D11Buffer* pCreativeSelectIB = nullptr;
    UINT creativeInventoryIndexCount = 0;
    UINT creativeSelectIndexCount = 0;

    bool mouseLocked = true;
    POINT mouseCenterPos;




    std::vector<ParkourObject> parkourObjects;
    std::vector<ParkourObject> parkourBalls;

    GameMode gameMode;

    bool CheckCameraCollision(const XMFLOAT3& cameraPos, float radius = 0.2f) const;

    bool CreateShaders();

    void CreateCube(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices, float x, float y, float z, float width, float height, float depth);
    bool CreateCubeMesh();

    bool keyProcessed[256] = { false };

    bool CreateFloorMesh();
    void HandleInput(float dt);
    void ResetKeyProcessing();
    void HandleConsoleInput(float dt);
    void HandleInventoryInput();
    void UpdateCamera();



    bool mouseStates[3] = { false, false, false }; // 0: left, 1: middle, 2: right
    float buildReach = 10.0f;

    bool RayIntersectsAABB(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir, const AABB& box, float& tNear);
    XMFLOAT3 GetPlacementPosition(const XMFLOAT3& hitPoint, const XMFLOAT3& normal);
    void HandleBuilding();

    bool IsBlockAtPosition(const XMFLOAT3& position, float tolerance = 0.1f) const;

    bool Create2DShaders();


    float breakCooldown = 0.0f;
    float placeCooldown = 0.0f;
    const float BREAK_DELAY = 0.3f;
    const float PLACE_DELAY = 0.2f; 

    void UpdateCooldowns(float dt) {
        if (breakCooldown > 0) breakCooldown -= dt;
        if (placeCooldown > 0) placeCooldown -= dt;
    };

private:

    HWND hWnd = nullptr;

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11RenderTargetView* pRTV = nullptr;
    ID3D11DepthStencilView* pDSV = nullptr;
    ID3D11VertexShader* pVS = nullptr;
    ID3D11PixelShader* pPS = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    ID3D11Buffer* pCB = nullptr;
    ID3D11SamplerState* pSampler = nullptr;

    //player
    ID3D11ShaderResourceView* pPlayerSRV;
    ID3D11Buffer* pPlayerVB;
    ID3D11Buffer* pPlayerIB;

    //inventory
    ID3D11Buffer* pInventoryVB;
    ID3D11Buffer* pInventoryIB;
    ID3D11Buffer* pInventorySelectVB;
    ID3D11Buffer* pInventorySelectIB;
    UINT inventoryIndexCount;
    UINT inventorySelectIndexCount;
    ID3D11ShaderResourceView* pWhiteTextureSRV;

    // Direct2D 
    ID2D1Factory* pD2D1Factory = nullptr;
    ID2D1RenderTarget* pD2D1RT = nullptr;
    ID2D1SolidColorBrush* pBrush = nullptr;
    IDWriteFactory* pDWriteFactory = nullptr;
    IDWriteTextFormat* pTextFormat = nullptr;

    // XAudio2 
    IXAudio2* pXAudio2;
    IXAudio2MasteringVoice* pMasterVoice;
    IXAudio2SourceVoice* pLandSound;

    ID3D11VertexShader* p2DVS = nullptr; 
    ID3D11PixelShader* p2DPS = nullptr; 
    ID3D11InputLayout* p2DInputLayout = nullptr; 
    ID3D11Buffer* pCrosshairVB = nullptr;
    ID3D11Buffer* pCrosshairIB = nullptr; 
    UINT crosshairIndexCount;
    ID3D11Buffer* p2DCB = nullptr; 
    ID3D11Buffer* pTextVB = nullptr; 
    ID3D11ShaderResourceView* pTextSRV = nullptr; 

    //---------------textures 
    ID3D11ShaderResourceView* pFloorSRV = nullptr;
    ID3D11ShaderResourceView* pCubeSRV = nullptr;
    ID3D11ShaderResourceView* pGrassSRV = nullptr;
    ID3D11ShaderResourceView* pStoneSRV = nullptr;
    ID3D11ShaderResourceView* pWoodSRV = nullptr;
    ID3D11ShaderResourceView* pMetalSRV = nullptr;
    ID3D11ShaderResourceView* pBrickSRV = nullptr;
    ID3D11ShaderResourceView* pDirtSRV = nullptr;
    ID3D11ShaderResourceView* pWaterSRV = nullptr;
    ID3D11ShaderResourceView* pLavaSRV = nullptr;
    //---------------textures 


    // skybox
    ID3D11RasterizerState* pRasterState = nullptr;

    XMFLOAT4X4 healthBarWorldMatrix;
    ID3D11Buffer* pHealthVB = nullptr;
    ID3D11Buffer* pHealthIB = nullptr;
    UINT healthIndexCount = 0;
    float health;

    ID3D11Buffer* pCubeVB = nullptr;
    ID3D11Buffer* pCubeIB = nullptr;
    ID3D11Buffer* pFloorVB = nullptr;
    ID3D11Buffer* pFloorIB = nullptr;
    UINT cubeIndexCount = 0;
    UINT floorIndexCount = 0;

    DirectX::XMMATRIX        worldMatrix;
    DirectX::XMMATRIX        viewMatrix;
    DirectX::XMMATRIX        projectionMatrix;

    XMFLOAT3                 playerPos;
    DirectX::XMFLOAT3        cameraPosition;
    DirectX::XMFLOAT3        cameraRotation; // pitch (x), yaw (y)

    std::vector<MapObject> mapObjects;

    // physics
    DirectX::XMFLOAT3 velocity;
    DirectX::XMFLOAT3 wishDir;
    DirectX::XMFLOAT3 acceleration;
    float gravity;
    bool isGrounded;
    bool flyMode;
    float jumpForce;
    bool wasGrounded;

    // movement 
    float walkSpeed;
    float runSpeed;
    float accelerationRate;
    float friction;
    float airAcceleration;
    float currentSpeed;
    float moveSpeed = 6.0f;
    float mouseSensitivity = 0.0025f;
    std::array<bool, 256> keyStates{};
    POINT lastMousePos{ 0,0 };
    bool firstMouse = true;

    bool consoleActive = false;
    std::string consoleInput;
    size_t historyIndex = 0;

    std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> commands;

    void ProcessCommand(const std::string& command);
    void RegisterCommands();
    void AddToHistory(const std::string& command);

};