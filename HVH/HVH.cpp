#include "framework.h"
#include "HVH.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include "GameEngine.h"
#include "Settings.h"
#include "Skybox.h"
#include <ShellScalingApi.h>
#include "Gamemod.h"
#pragma comment(lib, "Shcore.lib")

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

GameEngine* pGameEngine = nullptr;
bool isInGame = false;
bool isGamePaused = false;
bool isFullscreen = false;
bool isMenuState = true;
bool particlesInitialized = false;
HWND hWndMain = nullptr;

// Direct2D 
ID2D1Factory* pD2DFactory = nullptr;
ID2D1HwndRenderTarget* pRenderTarget = nullptr;
ID2D1SolidColorBrush* pTextBrush = nullptr;
ID2D1SolidColorBrush* pButtonBrush = nullptr;
ID2D1SolidColorBrush* pButtonHoverBrush = nullptr;
ID2D1SolidColorBrush* pConsoleBrush = nullptr;
ID2D1SolidColorBrush* pConsoleHeaderBrush = nullptr;
ID2D1SolidColorBrush* pConsoleTextBrush = nullptr;
IDWriteFactory* pDWriteFactory = nullptr;
IDWriteTextFormat* pTextFormat = nullptr;
ID2D1SolidColorBrush* pSliderShadowBrush = nullptr;

const float BUTTON_CORNER_RADIUS = 15.0f;
const float CONSOLE_CORNER_RADIUS = 15.0f;
const float CONSOLE_WIDTH = 600.0f;
const float CONSOLE_HEIGHT = 400.0f;
const float CONSOLE_HEADER_HEIGHT = 30.0f;
const float CONSOLE_OPACITY = 0.95f;
const float MENU_OPACITY = 0.85f;

const float CONSOLE_DEFAULT_WIDTH = 800.0f;
const float CONSOLE_DEFAULT_HEIGHT = 600.0f;
const float CONSOLE_PADDING = 15.0f;
const float CONSOLE_SCROLLBAR_WIDTH = 8.0f;
const float CONSOLE_FONT_SIZE_HEADER = 18.0f;
const float CONSOLE_FONT_SIZE_CONTENT = 14.0f;
const float CONSOLE_LINE_SPACING_FACTOR = 1.2f;

#undef COLOR_BACKGROUND
const D2D1_COLOR_F COLOR_BACKGROUND = D2D1::ColorF(0x1E1E1E, 0.95f);
const D2D1_COLOR_F COLOR_HEADER_BG = D2D1::ColorF(0x007ACC, 1.0f);
const D2D1_COLOR_F COLOR_BORDER = D2D1::ColorF(0x404040, 1.0f);
const D2D1_COLOR_F COLOR_TEXT = D2D1::ColorF(0xFFFFFF, 1.0f);
#undef COLOR_SCROLLBAR
const D2D1_COLOR_F COLOR_SCROLLBAR = D2D1::ColorF(0x808080, 0.7f);
const D2D1_COLOR_F COLOR_PROMPT = D2D1::ColorF(0x00FF00, 1.0f);

ID2D1SolidColorBrush* pBackgroundBrush = nullptr;
ID2D1SolidColorBrush* pHeaderBrush = nullptr;
ID2D1SolidColorBrush* pBorderBrush = nullptr;
ID2D1SolidColorBrush* pScrollbarBrush = nullptr;
ID2D1SolidColorBrush* pPromptBrush = nullptr;

float scrollPosition = 0.0f;

struct ConsoleLine {
    std::wstring text;
    float height;
};

float scrollOffset = 0.0f;

float consoleX = 0.0f;
float consoleY = 0.0f;
bool consoleDragging = false;
POINT dragStartPoint;

float animTime = 0.0f;

struct MenuButton {
    std::wstring text = L"";
    D2D1_RECT_F rect = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool isHovered = false;
    int id = 0;
    ButtonType type = ButtonType::Normal;
    float sliderValue = 0.0f;
};

GameMode currentGameMode = BUILD_MODE;

std::vector<MenuButton> menuButtons;
std::vector<MenuButton> pauseMenuButtons;
std::vector<MenuButton> settingsMenuButtons;
int hoveredButton = -1;
int currentResolutionIndex = 0;

std::vector<std::pair<int, int>> resolutions = {
    {800, 600},
    {1024, 768},
    {1280, 720},
    {1366, 768},
    {1920, 1080}
};

MenuState currentMenuState = MAIN_MENU;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HRESULT CreateGraphicsResources(HWND hWnd);
void DiscardGraphicsResources();
void DrawMenu(HWND hWnd, MenuState state);
void UpdateButtonHover(HWND hWnd, int x, int y, std::vector<MenuButton>& buttons);
void HandleButtonClick(HWND hWnd, int x, int y, std::vector<MenuButton>& buttons, MenuState currentState);
void ChangeResolution(HWND hWnd, int width, int height);
void InitializeMenuButtons(int windowWidth, int windowHeight);
void InitializePauseMenuButtons(int windowWidth, int windowHeight);
void InitializeSettingsMenuButtons(int windowWidth, int windowHeight);
void InitializeGameModeButtons(int windowWidth, int windowHeight);
void StartGame(HWND hWnd);
void UpdateAllMenuButtons(HWND hWnd);
void ToggleFullscreen(HWND hWnd);
void DrawConsoleOverlay(HWND hWnd, GameEngine* pGameEngine);
bool IsPointInConsoleHeader(int x, int y);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    currentMenuState = MAIN_MENU;
    isInGame = false;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HVH, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HVH));

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HVH));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    LoadSettings();

    HWND hWnd = CreateWindowW(szWindowClass, L"HVH Shooter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    hWndMain = hWnd;

    if (g_Settings.fullscreen)
    {
        isFullscreen = true;
        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hWnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED);
    }
    else
    {
        int width = resolutions[g_Settings.resolutionIndex].first;
        int height = resolutions[g_Settings.resolutionIndex].second;
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int x = (screenWidth - width) / 2;
        int y = (screenHeight - height) / 2;
        SetWindowPos(hWnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_FRAMECHANGED);
    }


    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

HRESULT CreateGraphicsResources(HWND hWnd)
{
    HRESULT hr = S_OK;

    if (pRenderTarget == nullptr)
    {
        RECT rc;
        GetClientRect(hWnd, &rc);

        UINT dpi = GetDpiForWindow(hWnd);
        FLOAT dpiX = static_cast<FLOAT>(dpi);
        FLOAT dpiY = static_cast<FLOAT>(dpi);

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
        if (SUCCEEDED(hr))
        {
            hr = pD2DFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), dpiX, dpiY),
                D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
                &pRenderTarget);
        }

        if (SUCCEEDED(hr))
        {
            hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&pDWriteFactory)
            );
        }

        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.3f), &pSliderShadowBrush);
        }

        if (SUCCEEDED(hr))
        {
            FLOAT fontSize = 24.0f * (dpiX / 96.0f); 
            hr = pDWriteFactory->CreateTextFormat(
                L"Arial",
                nullptr,
                DWRITE_FONT_WEIGHT_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                fontSize,
                L"",
                &pTextFormat
            );
        }

        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pTextBrush);
        }
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f, MENU_OPACITY), &pButtonBrush);
        }
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.4f, 0.4f, MENU_OPACITY), &pButtonHoverBrush);
        }
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f, CONSOLE_OPACITY), &pConsoleBrush);
        }
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.4f, CONSOLE_OPACITY), &pConsoleHeaderBrush);
        }
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pConsoleTextBrush);
        }
    }

    return hr;
}

void DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pTextBrush);
    SafeRelease(&pButtonBrush);
    SafeRelease(&pButtonHoverBrush);
    SafeRelease(&pConsoleBrush);
    SafeRelease(&pConsoleHeaderBrush);
    SafeRelease(&pConsoleTextBrush);
    SafeRelease(&pSliderShadowBrush);
    SafeRelease(&pTextFormat);
    SafeRelease(&pDWriteFactory);
    SafeRelease(&pD2DFactory);
    SafeRelease(&pBackgroundBrush);
    SafeRelease(&pHeaderBrush);
    SafeRelease(&pBorderBrush);
    SafeRelease(&pScrollbarBrush);
    SafeRelease(&pPromptBrush);
}

void StartMainLoop(HWND hWnd) {
    lastFrameTime = std::chrono::steady_clock::now();
    SetTimer(hWnd, 1, 16, NULL);
}

void StopMainLoop(HWND hWnd) {
    KillTimer(hWnd, 1);
}
void InitParticles(int width, int height, int count = 40) {
    if (width <= 0 || height <= 0) return;
    particles.clear();
    for (int i = 0; i < count; i++) {
        Particle p;
        p.x = (float)(rand() % width);
        p.y = (float)(rand() % height);
        p.vx = ((rand() % 200) - 100) / 50.0f; // speed
        p.vy = ((rand() % 200) - 100) / 50.0f;
        p.radius = 2.0f + (rand() % 300) / 100.0f; // rad
        particles.push_back(p);
    }
    particlesInitialized = true;
}

void UpdateAnimation(float deltaTime, int width, int height) {
    animTime += deltaTime;

    for (auto& p : particles) {
        p.x += p.vx * deltaTime * 60.0f;
        p.y += p.vy * deltaTime * 60.0f;
        if (p.x < 0 || p.x > width) p.vx = -p.vx;
        if (p.y < 0 || p.y > height) p.vy = -p.vy;
    }
}



void InitializeMenuButtons(int windowWidth, int windowHeight)
{
    menuButtons.clear();
    UINT dpi = GetDpiForWindow(hWndMain);
    float scale = static_cast<float>(dpi) / 96.0f;

    float buttonWidth = 300.0f * scale;
    float buttonHeight = 50.0f * scale;
    float startY = windowHeight * 0.3f;
    float spacing = 20.0f * scale;

    std::vector<std::wstring> buttonTexts = {
        L"НОВАЯ ИГРА",
        L"НАСТРОЙКИ",
        L"ВЫХОД"
    };

    for (int i = 0; i < buttonTexts.size(); ++i)
    {
        MenuButton button;
        button.text = buttonTexts[i];
        button.rect = D2D1::RectF(
            (windowWidth - buttonWidth) / 2,
            startY + i * (buttonHeight + spacing),
            (windowWidth + buttonWidth) / 2,
            startY + i * (buttonHeight + spacing) + buttonHeight
        );
        button.isHovered = false;
        button.id = i;
        menuButtons.push_back(button);
    }
}

void InitializeGameModeButtons(int windowWidth, int windowHeight)
{
    menuButtons.clear();
    UINT dpi = GetDpiForWindow(hWndMain);
    float scale = static_cast<float>(dpi) / 96.0f;

    float buttonWidth = 300.0f * scale;
    float buttonHeight = 50.0f * scale;
    float startY = windowHeight * 0.3f;
    float spacing = 20.0f * scale;

    std::vector<std::wstring> buttonTexts = {
        L"РЕЖИМ СТРОИТЕЛЬСТВА",
        L"РЕЖИМ ХВХ",
        L"НАЗАД"
    };

    for (int i = 0; i < buttonTexts.size(); ++i)
    {
        MenuButton button;
        button.text = buttonTexts[i];
        button.rect = D2D1::RectF(
            (windowWidth - buttonWidth) / 2,
            startY + i * (buttonHeight + spacing),
            (windowWidth + buttonWidth) / 2,
            startY + i * (buttonHeight + spacing) + buttonHeight
        );
        button.isHovered = false;
        button.id = i;
        menuButtons.push_back(button);
    }
}

void InitializePauseMenuButtons(int windowWidth, int windowHeight)
{
    pauseMenuButtons.clear();
    float buttonWidth = 300.0f;
    float buttonHeight = 50.0f;
    float startY = windowHeight * 0.3f;
    float spacing = 20.0f;

    std::vector<std::wstring> buttonTexts = {
        L"ПРОДОЛЖИТЬ",
        L"НАСТРОЙКИ",
        L"В ГЛАВНОЕ МЕНЮ",
        L"ВЫХОД"
    };

    for (int i = 0; i < buttonTexts.size(); ++i)
    {
        MenuButton button;
        button.text = buttonTexts[i];
        button.rect = D2D1::RectF(
            (windowWidth - buttonWidth) / 2,
            startY + i * (buttonHeight + spacing),
            (windowWidth + buttonWidth) / 2,
            startY + i * (buttonHeight + spacing) + buttonHeight
        );
        button.isHovered = false;
        button.id = i;
        pauseMenuButtons.push_back(button);
    }
}

void ToggleFullscreen(HWND hWnd)
{
    isFullscreen = !isFullscreen;

    if (isFullscreen)
    {
        RECT windowRect;
        GetWindowRect(hWnd, &windowRect);

        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hWnd, HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED);
    }
    else
    {
        SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        int width = resolutions[currentResolutionIndex].first;
        int height = resolutions[currentResolutionIndex].second;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int x = (screenWidth - width) / 2;
        int y = (screenHeight - height) / 2;

        SetWindowPos(hWnd, nullptr, x, y, width, height, SWP_FRAMECHANGED);
    }

    DiscardGraphicsResources();
    CreateGraphicsResources(hWnd);

    UpdateAllMenuButtons(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
}

void InitializeSettingsMenuButtons(int windowWidth, int windowHeight)
{
    settingsMenuButtons.clear();
    float buttonWidth = 300.0f;
    float buttonHeight = 50.0f;
    float startY = windowHeight * 0.3f;
    float spacing = 20.0f;

    MenuButton resButton;
    resButton.text = L"РАЗРЕШЕНИЕ: " +
        std::to_wstring(resolutions[g_Settings.resolutionIndex].first) +
        L"x" + std::to_wstring(resolutions[g_Settings.resolutionIndex].second);
    resButton.rect = D2D1::RectF((windowWidth - buttonWidth) / 2,
        startY,
        (windowWidth + buttonWidth) / 2,
        startY + buttonHeight);
    resButton.isHovered = false;
    resButton.id = 0;
    resButton.type = ButtonType::Normal;
    settingsMenuButtons.push_back(resButton);

    MenuButton fullscreenButton;
    fullscreenButton.text = g_Settings.fullscreen ? L"ПОЛНЫЙ ЭКРАН: ВКЛ" : L"ПОЛНЫЙ ЭКРАН: ВЫКЛ";
    fullscreenButton.rect = D2D1::RectF((windowWidth - buttonWidth) / 2,
        startY + buttonHeight + spacing,
        (windowWidth + buttonWidth) / 2,
        startY + buttonHeight + spacing + buttonHeight);
    fullscreenButton.isHovered = false;
    fullscreenButton.id = 1;
    fullscreenButton.type = ButtonType::Normal;
    settingsMenuButtons.push_back(fullscreenButton);

    MenuButton volumeButton;
    volumeButton.type = ButtonType::Slider;
    volumeButton.sliderValue = g_Settings.jumpVolume;
    volumeButton.text = L"ГРОМКОСТЬ ПРЫЖКА: " + std::to_wstring(static_cast<int>(g_Settings.jumpVolume * 100)) + L"%";
    volumeButton.rect = D2D1::RectF((windowWidth - buttonWidth) / 2,
        startY + 2 * (buttonHeight + spacing),
        (windowWidth + buttonWidth) / 2,
        startY + 2 * (buttonHeight + spacing) + buttonHeight);
    volumeButton.isHovered = false;
    volumeButton.id = 2;
    settingsMenuButtons.push_back(volumeButton);

    MenuButton backButton;
    backButton.text = L"НАЗАД";
    backButton.rect = D2D1::RectF((windowWidth - buttonWidth) / 2,
        startY + 3 * (buttonHeight + spacing),
        (windowWidth + buttonWidth) / 2,
        startY + 3 * (buttonHeight + spacing) + buttonHeight);
    backButton.isHovered = false;
    backButton.id = 3;
    backButton.type = ButtonType::Normal;
    settingsMenuButtons.push_back(backButton);
}

bool IsPointInConsoleHeader(int x, int y)
{
    UINT dpi = GetDpiForWindow(hWndMain);
    float scale = static_cast<float>(dpi) / 96.0f;
    float scaledWidth = CONSOLE_WIDTH * scale;
    float scaledHeader = CONSOLE_HEADER_HEIGHT * scale;

    int consoleXInt = static_cast<int>(std::round(consoleX));
    int consoleYInt = static_cast<int>(std::round(consoleY));
    int rightBound = static_cast<int>(std::round(consoleX + scaledWidth));
    int bottomBound = static_cast<int>(std::round(consoleY + scaledHeader));

    return (x >= consoleXInt && x <= rightBound &&
        y >= consoleYInt && y <= bottomBound);
}

void DrawConsoleOverlay(HWND hWnd, GameEngine* pGameEngine) {
    if (!pGameEngine || !pGameEngine->IsConsoleActive()) return;

    HRESULT hr = CreateGraphicsResources(hWnd);
    if (FAILED(hr)) return;

    pRenderTarget->BeginDraw();

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int windowWidth = clientRect.right - clientRect.left;
    int windowHeight = clientRect.bottom - clientRect.top;
    UINT dpi = GetDpiForWindow(hWnd);
    float dpiScale = static_cast<float>(dpi) / 96.0f;

    if (consoleX == 0.0f && consoleY == 0.0f) {
        consoleX = (windowWidth - CONSOLE_DEFAULT_WIDTH * dpiScale) / 2.0f;
        consoleY = (windowHeight - CONSOLE_DEFAULT_HEIGHT * dpiScale) / 2.0f;
    }

    float scaledWidth = CONSOLE_DEFAULT_WIDTH * dpiScale;
    float scaledHeight = CONSOLE_DEFAULT_HEIGHT * dpiScale;
    float scaledHeaderHeight = CONSOLE_HEADER_HEIGHT * dpiScale;
    float scaledCornerRadius = CONSOLE_CORNER_RADIUS * dpiScale;
    float scaledPadding = CONSOLE_PADDING * dpiScale;
    float scaledScrollbarWidth = CONSOLE_SCROLLBAR_WIDTH * dpiScale;

#undef max
#undef min
    consoleX = std::max(0.0f, std::min(consoleX, static_cast<float>(windowWidth) - scaledWidth));
    consoleY = std::max(0.0f, std::min(consoleY, static_cast<float>(windowHeight) - scaledHeight));

    if (!pBackgroundBrush) pRenderTarget->CreateSolidColorBrush(COLOR_BACKGROUND, &pBackgroundBrush);
    if (!pHeaderBrush) pRenderTarget->CreateSolidColorBrush(COLOR_HEADER_BG, &pHeaderBrush);
    if (!pBorderBrush) pRenderTarget->CreateSolidColorBrush(COLOR_BORDER, &pBorderBrush);
    if (!pTextBrush) pRenderTarget->CreateSolidColorBrush(COLOR_TEXT, &pTextBrush);
    if (!pScrollbarBrush) pRenderTarget->CreateSolidColorBrush(COLOR_SCROLLBAR, &pScrollbarBrush);
    if (!pPromptBrush) pRenderTarget->CreateSolidColorBrush(COLOR_PROMPT, &pPromptBrush);

    ID2D1SolidColorBrush* pCommandBrush = nullptr;
    ID2D1SolidColorBrush* pOutputBrush = nullptr;
    ID2D1SolidColorBrush* pAutocompleteBrush = nullptr;
    ID2D1SolidColorBrush* pErrorBrush = nullptr;
    ID2D1SolidColorBrush* pWarningBrush = nullptr;

    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f), &pCommandBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f), &pOutputBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.6f, 1.0f, 1.0f), &pAutocompleteBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.3f, 0.3f, 1.0f), &pErrorBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.8f, 0.2f, 1.0f), &pWarningBrush);

    D2D1_ROUNDED_RECT consoleRect = D2D1::RoundedRect(
        D2D1::RectF(consoleX, consoleY, consoleX + scaledWidth, consoleY + scaledHeight),
        scaledCornerRadius, scaledCornerRadius);
    pRenderTarget->FillRoundedRectangle(&consoleRect, pBackgroundBrush);
    pRenderTarget->DrawRoundedRectangle(&consoleRect, pBorderBrush, 1.5f * dpiScale);

    D2D1_RECT_F headerRect = D2D1::RectF(consoleX, consoleY, consoleX + scaledWidth, consoleY + scaledHeaderHeight);
    pRenderTarget->FillRectangle(headerRect, pHeaderBrush);

    IDWriteTextFormat* pHeaderFormat = nullptr;
    pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f * dpiScale, L"en-us", &pHeaderFormat);

    if (pHeaderFormat) {
        pHeaderFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        pHeaderFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        std::wstring title = L"Developer Console";
        D2D1_RECT_F titleRect = D2D1::RectF(consoleX + scaledPadding, consoleY, consoleX + scaledWidth - scaledPadding, consoleY + scaledHeaderHeight);
        pRenderTarget->DrawTextW(title.c_str(), static_cast<UINT32>(title.length()), pHeaderFormat, titleRect, pTextBrush);
        SafeRelease(&pHeaderFormat);
    }

    float contentTop = consoleY + scaledHeaderHeight + scaledPadding;
    float contentBottom = consoleY + scaledHeight - scaledPadding * 2 - (14.0f * dpiScale);
    float contentHeight = contentBottom - contentTop;
    float contentWidth = scaledWidth - scaledPadding * 2 - scaledScrollbarWidth;

    IDWriteTextFormat* pContentFormat = nullptr;
    pDWriteFactory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f * dpiScale, L"en-us", &pContentFormat);

    if (pContentFormat) {
        pContentFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        pContentFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        pContentFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

        const auto& history = pGameEngine->GetConsoleHistory();
        std::vector<ConsoleLine> wrappedLines;
        float totalContentHeight = 0.0f;

        for (const auto& line : history) {
            std::wstring wline(line.begin(), line.end());
            IDWriteTextLayout* pLayout = nullptr;
            pDWriteFactory->CreateTextLayout(wline.c_str(), static_cast<UINT32>(wline.length()), pContentFormat, contentWidth, FLT_MAX, &pLayout);
            if (pLayout) {
                DWRITE_TEXT_METRICS metrics;
                pLayout->GetMetrics(&metrics);
                wrappedLines.push_back({ wline, metrics.height });
                totalContentHeight += metrics.height;
                SafeRelease(&pLayout);
            }
        }

        float maxScroll = std::max(0.0f, totalContentHeight - contentHeight);
        scrollOffset = std::max(0.0f, std::min(scrollOffset, maxScroll));
        float currentScroll = scrollOffset;

        pRenderTarget->PushAxisAlignedClip(D2D1::RectF(consoleX + scaledPadding, contentTop, consoleX + scaledWidth - scaledPadding, contentBottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        float yPos = contentTop - currentScroll;

        for (size_t i = 0; i < wrappedLines.size(); ++i) {
            const auto& wrappedLine = wrappedLines[i];
            if (yPos + wrappedLine.height < contentTop) {
                yPos += wrappedLine.height;
                continue;
            }
            if (yPos > contentBottom) break;

            ID2D1SolidColorBrush* lineBrush = pOutputBrush;
            const std::string& originalLine = history[i];

            if (!originalLine.empty()) {
                if (originalLine[0] != ' ' && originalLine[0] != '\t') {
                    lineBrush = pCommandBrush;
                }
                else if (originalLine.find("ERROR") != std::string::npos || originalLine.find("Error") != std::string::npos) {
                    lineBrush = pErrorBrush;
                }
                else if (originalLine.find("WARNING") != std::string::npos || originalLine.find("Warning") != std::string::npos) {
                    lineBrush = pWarningBrush;
                }
            }

            D2D1_RECT_F lineRect = D2D1::RectF(consoleX + scaledPadding, yPos, consoleX + scaledWidth - scaledPadding - scaledScrollbarWidth, yPos + wrappedLine.height);

            if (lineBrush != nullptr && pRenderTarget != nullptr) {
                pRenderTarget->DrawText(wrappedLine.text.c_str(), static_cast<UINT32>(wrappedLine.text.length()), pContentFormat, lineRect, lineBrush);
            }
            yPos += wrappedLine.height;
        }
        pRenderTarget->PopAxisAlignedClip();

        const std::string& input = pGameEngine->GetConsoleInput();
        std::wstring inputW(input.begin(), input.end());
        std::wstring prompt = L"> " + inputW;

        std::wstring autocompleteHint = L"";
        if (!pGameEngine->currentTabMatches.empty() && !inputW.empty()) {
            size_t indexToShow = pGameEngine->currentTabCompletionIndex % pGameEngine->currentTabMatches.size();
            const std::string& fullMatch = pGameEngine->currentTabMatches[indexToShow];
            if (fullMatch.length() > input.length()) {
                autocompleteHint = std::wstring(fullMatch.begin() + input.length(), fullMatch.end());
            }
        }

        D2D1_RECT_F inputRect = D2D1::RectF(consoleX + scaledPadding, contentBottom + scaledPadding, consoleX + scaledWidth - scaledPadding, consoleY + scaledHeight - scaledPadding);
        pRenderTarget->DrawText(prompt.c_str(), static_cast<UINT32>(prompt.length()), pContentFormat, inputRect, pPromptBrush);

        if (!autocompleteHint.empty()) {
            std::wstring tempTextForWidth = L"> " + inputW;
            IDWriteTextLayout* pTempLayout = nullptr;
            pDWriteFactory->CreateTextLayout(tempTextForWidth.c_str(), static_cast<UINT32>(tempTextForWidth.length()), pContentFormat, contentWidth, FLT_MAX, &pTempLayout);
            float textWidth = 0.0f;
            if (pTempLayout) {
                DWRITE_TEXT_METRICS metrics;
                pTempLayout->GetMetrics(&metrics);
                textWidth = metrics.width;
                SafeRelease(&pTempLayout);
            }
            D2D1_RECT_F autocompleteRect = inputRect;
            autocompleteRect.left += textWidth;
            ID2D1SolidColorBrush* pFadedAutocompleteBrush = nullptr;
            pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.6f, 1.0f, 0.6f), &pFadedAutocompleteBrush);
            pRenderTarget->DrawText(autocompleteHint.c_str(), static_cast<UINT32>(autocompleteHint.length()), pContentFormat, autocompleteRect, pFadedAutocompleteBrush);
            SafeRelease(&pFadedAutocompleteBrush);
        }

        SafeRelease(&pContentFormat);

        if (totalContentHeight > contentHeight) {
            float scrollbarHeight = (contentHeight / totalContentHeight) * contentHeight;
            scrollbarHeight = std::max(scrollbarHeight, 15.0f * dpiScale);
            float scrollbarY = contentTop + (currentScroll / totalContentHeight) * contentHeight;
            D2D1_RECT_F scrollbarRect = D2D1::RectF(
                consoleX + scaledWidth - scaledScrollbarWidth - scaledPadding,
                scrollbarY,
                consoleX + scaledWidth - scaledPadding,
                scrollbarY + scrollbarHeight);
            pRenderTarget->FillRectangle(scrollbarRect, pScrollbarBrush);
        }
    }

    hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) DiscardGraphicsResources();

    SafeRelease(&pCommandBrush);
    SafeRelease(&pOutputBrush);
    SafeRelease(&pAutocompleteBrush);
    SafeRelease(&pErrorBrush);
    SafeRelease(&pWarningBrush);
}


void OnConsoleMouseWheel(int delta)
{
    float scrollStep = 20.0f;
    scrollOffset -= delta > 0 ? scrollStep : -scrollStep;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;

    if (pGameEngine)
    {
        float lineHeight = 20.0f;
#undef max
        float maxScroll = std::max(0.0f, pGameEngine->GetConsoleHistory().size() * lineHeight - (CONSOLE_HEIGHT - CONSOLE_HEADER_HEIGHT));
        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    }

    InvalidateRect(hWndMain, nullptr, FALSE);
}

void UpdateAllMenuButtons(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int windowWidth = rc.right - rc.left;
    int windowHeight = rc.bottom - rc.top;
    switch (currentMenuState)
    {
    case MAIN_MENU:
        InitializeMenuButtons(windowWidth, windowHeight);
        break;
    case GAME_MODE_SELECTION:
        InitializeGameModeButtons(windowWidth, windowHeight);
        break;
    case PAUSE_MENU:
        InitializePauseMenuButtons(windowWidth, windowHeight);
        break;
    case SETTINGS_MENU:
        InitializeSettingsMenuButtons(windowWidth, windowHeight);
        break;
    }
}

void DrawMenu(HWND hWnd, MenuState state)
{
    HRESULT hr = CreateGraphicsResources(hWnd);
    if (FAILED(hr)) return;
    pRenderTarget->BeginDraw();
    RECT rc;
    GetClientRect(hWnd, &rc);
    int windowWidth = rc.right - rc.left;
    int windowHeight = rc.bottom - rc.top;
    if (!particlesInitialized) {
        InitParticles(windowWidth, windowHeight, 50);
    }
    float t = sinf(animTime * 0.3f) * 0.5f + 0.5f;
    D2D1_COLOR_F color1 = D2D1::ColorF(0.1f + 0.2f * t, 0.1f, 0.3f + 0.2f * (1 - t), 1.0f);
    D2D1_COLOR_F color2 = D2D1::ColorF(0.05f, 0.1f + 0.25f * (1 - t), 0.15f + 0.35f * t, 1.0f);
    ID2D1LinearGradientBrush* pGradientBrush = nullptr;
    ID2D1GradientStopCollection* pGradientStops = nullptr;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].color = color1; stops[0].position = 0.0f;
    stops[1].color = color2; stops[1].position = 1.0f;
    hr = pRenderTarget->CreateGradientStopCollection(stops, 2,
        D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_MIRROR, &pGradientStops);
    if (SUCCEEDED(hr)) {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0, 0),
                D2D1::Point2F((float)windowWidth, (float)windowHeight));
        pRenderTarget->CreateLinearGradientBrush(props, pGradientStops, &pGradientBrush);
    }
    if (pGradientBrush) {
        pRenderTarget->FillRectangle(D2D1::RectF(0, 0, (float)windowWidth, (float)windowHeight), pGradientBrush);
    }
    SafeRelease(&pGradientBrush);
    SafeRelease(&pGradientStops);
    if (pTextBrush) {
        for (auto& p : particles) {
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F(p.x, p.y), p.radius, p.radius);
            pTextBrush->SetColor(D2D1::ColorF(1, 1, 1, 0.1f));
            pRenderTarget->FillEllipse(&ellipse, pTextBrush);
        }
    }
    UINT dpi = GetDpiForWindow(hWnd);
    float scale = static_cast<float>(dpi) / 96.0f;
    IDWriteTextFormat* pTitleTextFormat = nullptr;
    pDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        windowHeight * 0.07f * scale, L"", &pTitleTextFormat);
    if (pTitleTextFormat) {
        pTitleTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        pTitleTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        std::wstring title;
        switch (state) {
        case MAIN_MENU:      title = L"ХВХ!!!"; break;
        case GAME_MODE_SELECTION: title = L"ВЫБЕРИТЕ РЕЖИМ"; break;
        case PAUSE_MENU:     title = L"ПАУЗА"; break;
        case SETTINGS_MENU:  title = L"НАСТРОЙКИ"; break;
        default:             title = L""; break;
        }
        D2D1_RECT_F titleRect = D2D1::RectF(
            static_cast<FLOAT>(0),
            static_cast<FLOAT>(windowHeight) * 0.07f,
            static_cast<FLOAT>(windowWidth),
            static_cast<FLOAT>(windowHeight) * 0.2f
        );
        pTextBrush->SetColor(D2D1::ColorF(1, 1, 1, 0.95f));
        pRenderTarget->DrawTextW(title.c_str(), (UINT32)title.length(),
            pTitleTextFormat, titleRect, pTextBrush);
        SafeRelease(&pTitleTextFormat);
    }
    std::vector<MenuButton>* currentButtons = nullptr;
    if (state == MAIN_MENU)        currentButtons = &menuButtons;
    else if (state == GAME_MODE_SELECTION) currentButtons = &menuButtons;
    else if (state == PAUSE_MENU)  currentButtons = &pauseMenuButtons;
    else if (state == SETTINGS_MENU) currentButtons = &settingsMenuButtons;
    if (currentButtons) {
        for (const auto& button : *currentButtons) {
            if (button.type == ButtonType::Slider) {
                float sliderHeight = 45.0f;
                if (pSliderShadowBrush) {
                    D2D1_ROUNDED_RECT shadowRect = D2D1::RoundedRect(
                        D2D1::RectF(button.rect.left + 3, button.rect.top + 3,
                            button.rect.right + 3, button.rect.top + sliderHeight + 3),
                        BUTTON_CORNER_RADIUS, BUTTON_CORNER_RADIUS);
                    pRenderTarget->FillRoundedRectangle(shadowRect, pSliderShadowBrush);
                }
                ID2D1LinearGradientBrush* pSliderGradient = nullptr;
                ID2D1GradientStopCollection* pSliderStops = nullptr;
                D2D1_GRADIENT_STOP sliderStops[2] = {
                    {0.0f, D2D1::ColorF(0.2f, 0.2f, 0.25f, 1.0f)},
                    {1.0f, D2D1::ColorF(0.12f, 0.12f, 0.15f, 1.0f)}
                };
                pRenderTarget->CreateGradientStopCollection(sliderStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pSliderStops);
                if (pSliderStops) {
                    pRenderTarget->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(
                            D2D1::Point2F(button.rect.left, button.rect.top),
                            D2D1::Point2F(button.rect.right, button.rect.top)),
                        pSliderStops, &pSliderGradient);
                }
                if (pSliderGradient) {
                    D2D1_ROUNDED_RECT sliderRect = D2D1::RoundedRect(
                        D2D1::RectF(button.rect.left, button.rect.top, button.rect.right, button.rect.top + sliderHeight),
                        BUTTON_CORNER_RADIUS, BUTTON_CORNER_RADIUS);
                    pRenderTarget->FillRoundedRectangle(sliderRect, pSliderGradient);
                }
                SafeRelease(&pSliderStops);
                SafeRelease(&pSliderGradient);
                float filledWidth = (button.rect.right - button.rect.left) * button.sliderValue;
                D2D1_ROUNDED_RECT fillRect = D2D1::RoundedRect(
                    D2D1::RectF(button.rect.left, button.rect.top,
                        button.rect.left + filledWidth, button.rect.top + sliderHeight),
                    BUTTON_CORNER_RADIUS, BUTTON_CORNER_RADIUS);
                if (pButtonHoverBrush) {
                    pRenderTarget->FillRoundedRectangle(fillRect, pButtonHoverBrush);
                }
                std::wstring sliderText = L"ГРОМКОСТЬ ПРЫЖКА: " + std::to_wstring((int)(button.sliderValue * 100)) + L"%";
                IDWriteTextFormat* pSliderTextFormat = nullptr;
                pDWriteFactory->CreateTextFormat(
                    L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_MEDIUM,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    18.0f * scale, L"", &pSliderTextFormat);
                if (pSliderTextFormat) {
                    pSliderTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    pSliderTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    D2D1_RECT_F textRect = D2D1::RectF(button.rect.left, button.rect.top, button.rect.right, button.rect.top + sliderHeight);
                    pTextBrush->SetColor(D2D1::ColorF(1, 1, 1, 0.9f));
                    pRenderTarget->DrawTextW(sliderText.c_str(), (UINT32)sliderText.length(),
                        pSliderTextFormat, textRect, pTextBrush);
                    SafeRelease(&pSliderTextFormat);
                }
            }
            else {
                if (pSliderShadowBrush) {
                    D2D1_ROUNDED_RECT shadowRect = D2D1::RoundedRect(
                        D2D1::RectF(button.rect.left + 3, button.rect.top + 3,
                            button.rect.right + 3, button.rect.bottom + 3),
                        BUTTON_CORNER_RADIUS, BUTTON_CORNER_RADIUS);
                    pRenderTarget->FillRoundedRectangle(shadowRect, pSliderShadowBrush);
                }
                ID2D1LinearGradientBrush* pBtnGradient = nullptr;
                ID2D1GradientStopCollection* pBtnStops = nullptr;
                D2D1_GRADIENT_STOP btnStops[2] = {
                    {0.0f, button.isHovered ? D2D1::ColorF(0.3f, 0.5f, 0.9f, 1.0f) : D2D1::ColorF(0.25f, 0.25f, 0.35f, 1.0f)},
                    {1.0f, button.isHovered ? D2D1::ColorF(0.2f, 0.4f, 0.8f, 1.0f) : D2D1::ColorF(0.15f, 0.15f, 0.25f, 1.0f)}
                };
                pRenderTarget->CreateGradientStopCollection(btnStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pBtnStops);
                if (pBtnStops) {
                    pRenderTarget->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(
                            D2D1::Point2F(button.rect.left, button.rect.top),
                            D2D1::Point2F(button.rect.right, button.rect.bottom)),
                        pBtnStops, &pBtnGradient);
                }
                if (pBtnGradient) {
                    D2D1_ROUNDED_RECT buttonRoundedRect = D2D1::RoundedRect(button.rect, BUTTON_CORNER_RADIUS, BUTTON_CORNER_RADIUS);
                    pRenderTarget->FillRoundedRectangle(buttonRoundedRect, pBtnGradient);
                    pRenderTarget->DrawRoundedRectangle(buttonRoundedRect, pTextBrush, 2.0f);
                }
                SafeRelease(&pBtnStops);
                SafeRelease(&pBtnGradient);
                IDWriteTextFormat* pButtonTextFormat = nullptr;
                pDWriteFactory->CreateTextFormat(
                    L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    20.0f * scale, L"", &pButtonTextFormat);
                if (pButtonTextFormat) {
                    pButtonTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    pButtonTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    pTextBrush->SetColor(D2D1::ColorF(1, 1, 1, 0.95f));
                    pRenderTarget->DrawTextW(button.text.c_str(), (UINT32)button.text.length(),
                        pButtonTextFormat, button.rect, pTextBrush);
                    SafeRelease(&pButtonTextFormat);
                }
            }
        }
    }
    hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) DiscardGraphicsResources();
}

void UpdateButtonHover(HWND hWnd, int x, int y, std::vector<MenuButton>& buttons)
{
    UINT dpi = GetDpiForWindow(hWnd);
    float scale = static_cast<float>(dpi) / 96.0f;
    float scaledX = static_cast<float>(x) / scale;
    float scaledY = static_cast<float>(y) / scale;
    bool needRedraw = false;
    for (auto& button : buttons)
    {
        bool wasHovered = button.isHovered;
        button.isHovered = (scaledX >= button.rect.left && scaledX <= button.rect.right &&
            scaledY >= button.rect.top && scaledY <= button.rect.bottom);
        if (wasHovered != button.isHovered)
        {
            needRedraw = true;
        }
    }
    if (needRedraw)
    {
        InvalidateRect(hWnd, nullptr, FALSE);
    }
}

void StartGame(HWND hWnd)
{
    if (pGameEngine)
    {
        pGameEngine->Cleanup();
        delete pGameEngine;
        pGameEngine = nullptr;
    }

    RECT rc;
    GetClientRect(hWnd, &rc);

    isInGame = true;
    currentMenuState = IN_GAME;
    pGameEngine = new GameEngine(hWnd);

    if (!pGameEngine->Initialize())
    {
        MessageBox(hWnd, L"Ошибка инициализации игры!", L"Error", MB_OK);
        delete pGameEngine;
        pGameEngine = nullptr;
        isInGame = false;
        currentMenuState = MAIN_MENU;
        InitializeMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
    }
    else
    {
        pGameEngine->SetGameMode(currentGameMode);
        ShowCursor(FALSE);
    }
}

void HandleButtonClick(HWND hWnd, int x, int y, std::vector<MenuButton>& buttons, MenuState currentState)
{
    UINT dpi = GetDpiForWindow(hWnd);
    float scale = static_cast<float>(dpi) / 96.0f;
    float scaledX = static_cast<float>(x) / scale;
    float scaledY = static_cast<float>(y) / scale;
    RECT rc;
    GetClientRect(hWnd, &rc);
    for (auto& button : buttons)
    {
        if (x < button.rect.left || scaledX > button.rect.right ||
            y < button.rect.top || scaledY > button.rect.bottom)
            continue;
        switch (currentState)
        {
        case MAIN_MENU:
            if (button.id == 0)
            {
                currentMenuState = GAME_MODE_SELECTION;
                InitializeGameModeButtons(rc.right - rc.left, rc.bottom - rc.top);
            }
            else if (button.id == 1)
            {
                currentMenuState = SETTINGS_MENU;
                InitializeSettingsMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
                SaveSettings();
            }
            else if (button.id == 2)
            {
                PostQuitMessage(0);
            }
            break;
        case GAME_MODE_SELECTION:
            if (button.id == 0) 
            {
                currentGameMode = BUILD_MODE;
                StartGame(hWnd);
            }
            else if (button.id == 1) 
            {
                currentGameMode = HVH_MODE;
                StartGame(hWnd);
            }
            else if (button.id == 2) // back
            {
                currentMenuState = MAIN_MENU;
                InitializeMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
            }
            break;
        case PAUSE_MENU:
            if (button.id == 0)
            {
                currentMenuState = IN_GAME;
                ShowCursor(FALSE);
                SaveSettings();
            }
            else if (button.id == 1)
            {
                currentMenuState = SETTINGS_MENU;
                InitializeSettingsMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
                SaveSettings();
            }
            else if (button.id == 2)
            {
                currentMenuState = MAIN_MENU;
                isInGame = false;
                if (pGameEngine)
                {
                    pGameEngine->Cleanup();
                    delete pGameEngine;
                    pGameEngine = nullptr;
                }
                ShowCursor(TRUE);
                RECT rc;
                GetClientRect(hWnd, &rc);
                InitializeMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
                SaveSettings();
            }
            else if (button.id == 3)
            {
                SaveSettings();
                PostQuitMessage(0);
            }
            break;
        case SETTINGS_MENU:
            if (button.id == 0)
            {
                currentResolutionIndex = (currentResolutionIndex + 1) % resolutions.size();
                g_Settings.resolutionIndex = currentResolutionIndex;
                if (!isFullscreen)
                {
                    ChangeResolution(hWnd,
                        resolutions[currentResolutionIndex].first,
                        resolutions[currentResolutionIndex].second);
                }
                else
                {
                    InitializeSettingsMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                SaveSettings();
            }
            else if (button.id == 1)
            {
                ToggleFullscreen(hWnd);
                g_Settings.fullscreen = isFullscreen;
                SaveSettings();
            }
            else if (button.id == 2)
            {
                SaveSettings();
            }
            else if (button.id == 3)
            {
                currentMenuState = isInGame ? PAUSE_MENU : MAIN_MENU;
                UpdateAllMenuButtons(hWnd);
            }
            break;
        }
        break;
    }
}
void ChangeResolution(HWND hWnd, int width, int height)
{
    if (isFullscreen)
    {
        isFullscreen = false;
        SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    }

    SetWindowPos(hWnd, nullptr, 0, 0, width, height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

    RECT rc;
    GetWindowRect(hWnd, &rc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    SetWindowPos(hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    DiscardGraphicsResources();
    CreateGraphicsResources(hWnd);

    UpdateAllMenuButtons(hWnd);
    InvalidateRect(hWnd, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER lastTime;
    static bool timerInitialized = false;
    switch (message)
    {
    case WM_CREATE:
    {
        menuButtons.clear();
        pauseMenuButtons.clear();
        settingsMenuButtons.clear();

        RECT rc;
        GetClientRect(hWnd, &rc);
        InitializeMenuButtons(rc.right - rc.left, rc.bottom - rc.top);

        StartMainLoop(hWnd);
        isMenuState = (currentMenuState != IN_GAME);
        SetTimer(hWnd, 1, 5, nullptr);
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&lastTime);
        timerInitialized = true;
        return 0;
    }
    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        if (pRenderTarget)
            pRenderTarget->Resize(D2D1::SizeU(rc.right, rc.bottom));
        if (isInGame && pGameEngine && currentMenuState == IN_GAME)
            pGameEngine->OnResize(rc.right - rc.left, rc.bottom - rc.top);
        if (currentMenuState == MAIN_MENU)
            InitializeMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
        else if (currentMenuState == GAME_MODE_SELECTION)
            InitializeGameModeButtons(rc.right - rc.left, rc.bottom - rc.top);
        else if (currentMenuState == PAUSE_MENU)
            InitializePauseMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
        else if (currentMenuState == SETTINGS_MENU)
            InitializeSettingsMenuButtons(rc.right - rc.left, rc.bottom - rc.top);

        if (currentMenuState != IN_GAME && particlesInitialized)
        {
            InitParticles(rc.right - rc.left, rc.bottom - rc.top);
        }
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (pGameEngine && pGameEngine->IsConsoleActive())
        {
            OnConsoleMouseWheel(delta);
            return 0;
        }
        return 0;
    }
    case WM_CHAR:
    {
        if (pGameEngine && pGameEngine->IsConsoleActive())
        {
            if (static_cast<UINT>(wParam) != 96)
            {
                pGameEngine->OnChar(static_cast<UINT>(wParam));
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        if (pGameEngine && currentMenuState == IN_GAME && wParam == VK_OEM_3) // `
        {
            pGameEngine->ToggleConsole();
            InvalidateRect(hWnd, nullptr, FALSE);
            if (pGameEngine->IsConsoleActive()) {
                pGameEngine->currentTabMatches.clear();
                pGameEngine->currentTabCompletionIndex = 0;
            }
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            if (pGameEngine && pGameEngine->IsConsoleActive())
            {
                pGameEngine->ToggleConsole();
                pGameEngine->currentTabMatches.clear();
                pGameEngine->currentTabCompletionIndex = 0;
            }
            else if (currentMenuState == IN_GAME)
            {
                currentMenuState = PAUSE_MENU;
                ShowCursor(TRUE);
                RECT rc;
                GetClientRect(hWnd, &rc);
                InitializePauseMenuButtons(rc.right - rc.left, rc.bottom - rc.top);
            }
            else if (currentMenuState == PAUSE_MENU || currentMenuState == SETTINGS_MENU)
            {
                currentMenuState = IN_GAME;
                ShowCursor(FALSE);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (pGameEngine && pGameEngine->IsConsoleActive())
        {
            if (wParam == VK_UP || wParam == VK_DOWN)
            {
                pGameEngine->CycleConsoleHistory(wParam == VK_UP);
                pGameEngine->currentTabMatches.clear();
                pGameEngine->currentTabCompletionIndex = 0;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (wParam == VK_TAB)
            {
                const std::string& currentInput = pGameEngine->GetConsoleInput();
                const auto& commands = pGameEngine->GetCommands();
                if (currentInput != pGameEngine->currentTabCompletionPrefix || pGameEngine->currentTabMatches.empty()) {
                    pGameEngine->currentTabCompletionPrefix = currentInput;
                    pGameEngine->currentTabMatches.clear();
                    pGameEngine->currentTabCompletionIndex = 0;
                    for (const auto& cmd : commands) {
                        if (cmd.first.length() >= currentInput.length() &&
                            cmd.first.substr(0, currentInput.length()) == currentInput) {
                            pGameEngine->currentTabMatches.push_back(cmd.first);
                        }
                    }
                    if (!pGameEngine->currentTabMatches.empty()) {
                        std::sort(pGameEngine->currentTabMatches.begin(), pGameEngine->currentTabMatches.end());
                    }
                }
                if (!pGameEngine->currentTabMatches.empty()) {
                    pGameEngine->currentTabCompletionIndex = (pGameEngine->currentTabCompletionIndex + 1) % pGameEngine->currentTabMatches.size();
                    pGameEngine->SetConsoleInput(pGameEngine->currentTabMatches[pGameEngine->currentTabCompletionIndex]);
                }
                else {
                    // total pox nax!
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (wParam != VK_TAB && wParam != VK_UP && wParam != VK_DOWN &&
                wParam != VK_LEFT && wParam != VK_RIGHT && wParam != VK_BACK &&
                wParam != VK_DELETE && wParam != VK_RETURN && wParam != VK_SHIFT &&
                wParam != VK_CONTROL && wParam != VK_MENU) {
                pGameEngine->currentTabMatches.clear();
                pGameEngine->currentTabCompletionIndex = 0;
            }
            if (wParam == VK_BACK || wParam == VK_DELETE) {
                pGameEngine->currentTabMatches.clear();
                pGameEngine->currentTabCompletionIndex = 0;
            }
        }
        if (isInGame && pGameEngine && currentMenuState == IN_GAME)
            pGameEngine->OnKeyDown(static_cast<UINT>(wParam));
        return 0;
    }
    case WM_KEYUP:
    {
        if (isInGame && pGameEngine && currentMenuState == IN_GAME)
            pGameEngine->OnKeyUp(static_cast<UINT>(wParam));
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (pGameEngine && pGameEngine->IsConsoleActive() && consoleDragging)
        {
            consoleX = static_cast<float>(std::round(static_cast<double>(x - dragStartPoint.x)));
            consoleY = static_cast<float>(std::round(static_cast<double>(y - dragStartPoint.y)));
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (currentMenuState == IN_GAME && pGameEngine)
        {
            if (!pGameEngine->IsConsoleActive())
            {
                pGameEngine->OnMouseMove(x, y, true);
                SetCursor(nullptr);
            }
            else
            {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }
        }
        else
        {
            std::vector<MenuButton>* buttons = nullptr;
            switch (currentMenuState)
            {
            case MAIN_MENU:
                buttons = &menuButtons;
                break;
            case GAME_MODE_SELECTION:
                buttons = &menuButtons; 
                break;
            case PAUSE_MENU:
                buttons = &pauseMenuButtons;
                break;
            case SETTINGS_MENU:
                buttons = &settingsMenuButtons;
                break;
            }
            if (buttons)
            {
                bool needRedraw = false;
                for (auto& button : *buttons)
                {
                    bool wasHovered = button.isHovered;
                    if (button.type == ButtonType::Slider)
                    {
                        float sliderHeight = 50.0f;
                        D2D1_RECT_F sliderRect = D2D1::RectF(button.rect.left, button.rect.top,
                            button.rect.right, button.rect.top + sliderHeight);
                        button.isHovered = (x >= sliderRect.left && x <= sliderRect.right &&
                            y >= sliderRect.top && y <= sliderRect.bottom);
                        if (wParam & MK_LBUTTON && button.isHovered)
                        {
                            button.sliderValue = static_cast<float>(x - sliderRect.left) /
                                (sliderRect.right - sliderRect.left);
                            if (button.sliderValue < 0.0f) button.sliderValue = 0.0f;
                            if (button.sliderValue > 1.0f) button.sliderValue = 1.0f;
                            g_Settings.jumpVolume = button.sliderValue;
                            if (pGameEngine) pGameEngine->SetJumpVolume(button.sliderValue);
                            button.text = L"ГРОМКОСТЬ ПРЫЖКА: " + std::to_wstring(static_cast<int>(button.sliderValue * 100)) + L"%";
                            SaveSettings();
                            needRedraw = true;
                        }
                    }
                    else
                    {
                        button.isHovered = (x >= button.rect.left && x <= button.rect.right &&
                            y >= button.rect.top && y <= button.rect.bottom);
                    }
                    if (button.isHovered != wasHovered) needRedraw = true;
                }
                if (needRedraw) InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (pGameEngine && pGameEngine->IsConsoleActive() && IsPointInConsoleHeader(x, y))
        {
            consoleDragging = true;
            dragStartPoint.x = static_cast<LONG>(std::round(x - consoleX));
            dragStartPoint.y = static_cast<LONG>(std::round(y - consoleY));
            SetCapture(hWnd);
            return 0;
        }
        if (pGameEngine && currentMenuState == IN_GAME && !pGameEngine->IsConsoleActive())
        {
            pGameEngine->OnMouseDown(0);
        }
        else if (currentMenuState == MAIN_MENU)
        {
            HandleButtonClick(hWnd, x, y, menuButtons, MAIN_MENU);
        }
        else if (currentMenuState == GAME_MODE_SELECTION)
        {
            HandleButtonClick(hWnd, x, y, menuButtons, GAME_MODE_SELECTION);
        }
        else if (currentMenuState == PAUSE_MENU)
        {
            HandleButtonClick(hWnd, x, y, pauseMenuButtons, PAUSE_MENU);
        }
        else if (currentMenuState == SETTINGS_MENU)
        {
            HandleButtonClick(hWnd, x, y, settingsMenuButtons, SETTINGS_MENU);
        }
        return 0;
    }
    case WM_RBUTTONDOWN:
    {
        if (pGameEngine && currentMenuState == IN_GAME && !pGameEngine->IsConsoleActive())
        {
            pGameEngine->OnMouseDown(2);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        if (consoleDragging)
        {
            consoleDragging = false;
            ReleaseCapture();
        }
        if (pGameEngine && currentMenuState == IN_GAME && !pGameEngine->IsConsoleActive())
        {
            pGameEngine->OnMouseUp(0);
        }
        return 0;
    }
    case WM_RBUTTONUP:
    {
        if (pGameEngine && currentMenuState == IN_GAME && !pGameEngine->IsConsoleActive())
        {
            pGameEngine->OnMouseUp(2);
        }
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        if (pGameEngine && currentMenuState == IN_GAME)
        {
            pGameEngine->Render();
            if (pGameEngine->IsConsoleActive())
            {
                DrawConsoleOverlay(hWnd, pGameEngine);
            }
        }
        else
        {
            DrawMenu(hWnd, currentMenuState);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_TIMER:
    {
        // static LARGE_INTEGER frequency;
        // static LARGE_INTEGER lastTime;
        // static bool timerInitialized = false;
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        float dt = 1.0f;
        if (timerInitialized) {
            dt = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
        }
        lastTime = currentTime;
        timerInitialized = true;
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (currentMenuState == IN_GAME && pGameEngine)
        {
            UpdateAnimation(dt, w, h);
            pGameEngine->Update(dt / 3.25f);
            if (!pGameEngine->IsConsoleActive()) {
                InvalidateRect(hWnd, nullptr, FALSE);
                ShowCursor(TRUE);
            }
        }
        else if (currentMenuState != IN_GAME)
        {
            UpdateAnimation(dt, w, h);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_DESTROY:
    {
        if (pGameEngine)
        {
            pGameEngine->Cleanup();
            delete pGameEngine;
            pGameEngine = nullptr;
        }
        StopMainLoop(hWnd);
        DiscardGraphicsResources();
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    case WM_SETCURSOR:
    {
        if (currentMenuState == IN_GAME && pGameEngine && !pGameEngine->IsConsoleActive())
        {
            SetCursor(nullptr);
            return 1;
        }
        break;
    }
    case WM_ACTIVATE:
    {
        if (wParam == WA_INACTIVE)
            ShowCursor(TRUE);
        return 0;
    }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}