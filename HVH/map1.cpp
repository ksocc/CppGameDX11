#include "map1.h"
#include <fstream>
#include <sstream>
#include <string>
#include <shlobj.h> // For SHGetFolderPath
#include <windows.h>

// Use filesystem if available (C++17 or later)
#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
// Fallback for older compilers: construct path manually
#include <string>
#endif

std::vector<ParkourObject> ParkourMap::CreateParkourCourse() {
    // Return an empty vector for the parkour course
    return std::vector<ParkourObject>();
}

void ParkourMap::AddParkourObject(std::vector<ParkourObject>& course, const ParkourObject& object) {
    // Add a single object to the course
    course.push_back(object);
}

bool ParkourMap::SaveParkourCourse(const std::vector<ParkourObject>& course, const std::string& filename) {
    // Get the Documents folder path
    char documentsPath[MAX_PATH];
    HRESULT result = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, documentsPath);
    if (FAILED(result)) {
        return false;
    }

    // Construct the full path: Documents/HVHproject/filename
#if __cplusplus >= 201703L
    fs::path filePath = fs::path(documentsPath) / "HVHproject" / filename;
#else
    // Fallback: manual path construction
    std::string filePath = std::string(documentsPath) + "\\HVHproject\\" + filename;
#endif

    // Open file for writing
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        return false;
    }

    // Write each object to the file
    for (const auto& obj : course) {
        outFile << obj.type << " "
            << obj.position.x << " " << obj.position.y << " " << obj.position.z << " "
            << obj.rotation.x << " " << obj.rotation.y << " " << obj.rotation.z << " "
            << obj.scale.x << " " << obj.scale.y << " " << obj.scale.z << " "
            << obj.color.x << " " << obj.color.y << " " << obj.color.z << " " << obj.color.w << "\n";
    }

    outFile.close();
    return true;
}

std::vector<ParkourObject> ParkourMap::LoadParkourCourse(const std::string& filename) {
    std::vector<ParkourObject> course;

    // Get the Documents folder path
    char documentsPath[MAX_PATH];
    HRESULT result = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, documentsPath);
    if (FAILED(result)) {
        return course; // Return empty course on error
    }

    // Construct the full path: Documents/HVHproject/filename
#if __cplusplus >= 201703L
    fs::path filePath = fs::path(documentsPath) / "HVHproject" / filename;
#else
    // Fallback: manual path construction
    std::string filePath = std::string(documentsPath) + "\\HVHproject\\" + filename;
#endif

    // Open file for reading
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        return course; // Return empty course if file cannot be opened
    }

    // Read each line and parse into ParkourObject
    std::string line;
    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        ParkourObject obj;
        iss >> obj.type
            >> obj.position.x >> obj.position.y >> obj.position.z
            >> obj.rotation.x >> obj.rotation.y >> obj.rotation.z
            >> obj.scale.x >> obj.scale.y >> obj.scale.z
            >> obj.color.x >> obj.color.y >> obj.color.z >> obj.color.w;
        if (iss.fail()) {
            continue; // Skip malformed lines
        }
        course.push_back(obj);
    }

    inFile.close();
    return course;
}

std::vector<ParkourObject> ParkourMap::CreateParkourBalls() {
    std::vector<ParkourObject> balls;

    for (int i = 0; i < 12; ++i) {
        ParkourObject metalBall;
        metalBall.position = { float((i % 4) * 2 - 3), 6.0f + i * 0.5f, 15.0f + i * 5.0f };
        metalBall.rotation = { 0.0f, 0.0f, 0.0f };
        metalBall.scale = { 0.7f, 0.7f, 0.7f };
        metalBall.type = "metal";
        metalBall.color = { 0.8f, 0.8f, 0.8f, 1.0f };
        balls.push_back(metalBall);
    }

    for (int i = 0; i < 8; ++i) {
        ParkourObject lavaBall;
        lavaBall.position = { float(-3 + i % 3), 8.0f + i * 0.6f, 50.0f + i * 4.0f };
        lavaBall.rotation = { 0.0f, 0.0f, 0.0f };
        lavaBall.scale = { 0.9f, 0.9f, 0.9f };
        lavaBall.type = "lava";
        lavaBall.color = { 1.0f, 0.4f, 0.1f, 1.0f };
        balls.push_back(lavaBall);
    }

    for (int i = 0; i < 6; ++i) {
        ParkourObject waterBall;
        waterBall.position = { float(i % 2 * 3 - 1.5f), 7.0f + i * 0.4f, 80.0f + i * 5.0f };
        waterBall.rotation = { 0.0f, 0.0f, 0.0f };
        waterBall.scale = { 0.8f, 0.8f, 0.8f };
        waterBall.type = "water";
        waterBall.color = { 0.1f, 0.6f, 1.0f, 0.8f };
        balls.push_back(waterBall);
    }

    for (int i = 0; i < 4; ++i) {
        ParkourObject hazardBall;
        hazardBall.position = { float(-2 + i % 2), 10.0f + i * 0.7f, 100.0f + i * 3.0f };
        hazardBall.rotation = { 0.0f, 0.0f, 0.0f };
        hazardBall.scale = { 1.0f, 1.0f, 1.0f };
        hazardBall.type = "lava";
        hazardBall.color = { 1.0f, 0.5f, 0.0f, 1.0f };
        balls.push_back(hazardBall);
    }

    return balls;
}