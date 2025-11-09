#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>

using namespace DirectX;

struct ParkourObject {
    XMFLOAT3 position;
    XMFLOAT3 rotation;
    XMFLOAT3 scale;
    std::string type;
    XMFLOAT4 color;

    ParkourObject()
        : position{ 0.0f, 0.0f, 0.0f }
        , rotation{ 0.0f, 0.0f, 0.0f }
        , scale{ 1.0f, 1.0f, 1.0f }
        , type("")
        , color{ 1.0f, 1.0f, 1.0f, 1.0f }
    {
    }
};

class ParkourMap {
public:
    static std::vector<ParkourObject> CreateParkourCourse();
    static void AddParkourObject(std::vector<ParkourObject>& course, const ParkourObject& object);
    static bool SaveParkourCourse(const std::vector<ParkourObject>& course, const std::string& filename);
    static std::vector<ParkourObject> LoadParkourCourse(const std::string& filename);
    static std::vector<ParkourObject> CreateParkourBalls();
};