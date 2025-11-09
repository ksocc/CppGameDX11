#pragma once
#include "resource.h"
#include "SafeRelease.h"
#include "Gamemod.h" 

struct Particle {
    float x, y;
    float vx, vy;
    float radius;
};

enum class ButtonType {
    Normal,
    Slider
};

enum MenuState {
    MAIN_MENU,
    GAME_MODE_SELECTION, 
    IN_GAME,
    PAUSE_MENU,
    SETTINGS_MENU
};

extern GameMode currentGameMode;

std::vector<Particle> particles;
std::chrono::steady_clock::time_point lastFrameTime;
