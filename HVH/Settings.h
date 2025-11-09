#pragma once
#include <windows.h>
#include <ShlObj.h>
#include <string>
#include <fstream>

#pragma comment(lib, "Shell32.lib")

struct Settings {
    float jumpVolume = 0.7f;
    bool fullscreen = false;
    int resolutionIndex = 2;
};

extern Settings g_Settings;

inline void XorCrypt(std::string& data, const char* key, size_t keyLen) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % keyLen];
    }
}

inline void SaveSettings() {
    WCHAR docPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, docPath))) {
        std::wstring folderPath = std::wstring(docPath) + L"\\HVHproject";
        std::wstring filePath = folderPath + L"\\HVHSettings.bin";
        CreateDirectoryW(folderPath.c_str(), NULL);
        std::ofstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            std::string data = std::to_string(g_Settings.jumpVolume) + ";" +
                std::to_string(g_Settings.fullscreen ? 1 : 0) + ";" +
                std::to_string(g_Settings.resolutionIndex);
            const char* key = "decodernax";
            XorCrypt(data, key, strlen(key));
            file.write(data.data(), data.size());
            file.close();
        }
    }
}

inline void LoadSettings() {
    WCHAR docPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, docPath))) {
        std::wstring filePath = std::wstring(docPath) + L"\\HVHproject\\HVHSettings.bin";
        std::ifstream file(filePath, std::ios::binary);
        if (file.is_open()) {
            std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            const char* key = "decodernax";
            XorCrypt(data, key, strlen(key));
            size_t pos = 0;
            size_t next = data.find(';');
            if (next != std::string::npos) {
                g_Settings.jumpVolume = std::stof(data.substr(0, next));
                pos = next + 1;
                next = data.find(';', pos);
                if (next != std::string::npos) {
                    g_Settings.fullscreen = data.substr(pos, next - pos) == "1";
                    pos = next + 1;
                    g_Settings.resolutionIndex = std::stoi(data.substr(pos));
                }
            }
            if (g_Settings.jumpVolume < 0.0f) g_Settings.jumpVolume = 0.0f;
            if (g_Settings.jumpVolume > 1.0f) g_Settings.jumpVolume = 1.0f;
            if (g_Settings.resolutionIndex < 0 || g_Settings.resolutionIndex >= 5)
                g_Settings.resolutionIndex = 2;
        }
    }
}
