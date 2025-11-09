#include "GameEngine.h"

void GameEngine::CycleConsoleHistory(bool up) {
    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
    if (consoleHistory.empty()) return;

    if (up) {
        if (historyIndex < consoleHistory.size() - 1) {
            historyIndex++;
        }
    }
    else {
        if (historyIndex > 0) {
            historyIndex--;
        }
    }

    if (historyIndex < consoleHistory.size()) {
        consoleInput = consoleHistory[consoleHistory.size() - 1 - historyIndex];
        // consoleInput = userCommandHistory[userCommandHistory.size() - 1 - historyIndex];
    }
    else {
        consoleInput.clear();
    }
}