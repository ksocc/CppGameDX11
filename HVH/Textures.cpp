#include "GameEngine.h"

//bool GameEngine::CreateBrickTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV) {
//    std::vector<BYTE> data(size * size * 4);
//    UINT brickHeight = size / 8;
//    for (UINT y = 0; y < size; ++y) {
//        for (UINT x = 0; x < size; ++x) {
//            UINT index = (y * size + x) * 4;
//            bool mortar = ((y % brickHeight) == 0) || ((x % (brickHeight * 2)) == 0);
//            if (mortar) {
//                data[index] = 200; data[index + 1] = 200; data[index + 2] = 200; 
//            }
//            else {
//                data[index] = 50; data[index + 1] = 50; data[index + 2] = 200; 
//            }
//            data[index + 3] = 255;
//        }
//    }
//    return CreateTextureFromData(data.data(), size, size, outSRV);
//}

bool GameEngine::CreateWaterTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV) {
    std::vector<BYTE> data(size * size * 4);
    for (UINT y = 0; y < size; ++y) {
        for (UINT x = 0; x < size; ++x) {
            UINT index = (y * size + x) * 4;
            BYTE wave = static_cast<BYTE>(sinf(x * 0.1f + y * 0.05f) * 50 + 100);
            data[index] = 200;        // B
            data[index + 1] = wave;   // G
            data[index + 2] = wave / 2; // R
            data[index + 3] = 200;    // A
        }
    }
    return CreateTextureFromData(data.data(), size, size, outSRV);
}

bool GameEngine::CreateLavaTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV) {
    std::vector<BYTE> data(size * size * 4);
    for (UINT y = 0; y < size; ++y) {
        for (UINT x = 0; x < size; ++x) {
            UINT index = (y * size + x) * 4;
            BYTE heat = static_cast<BYTE>(200 + sinf(x * 0.15f) * 55);
            data[index] = 0;          // B
            data[index + 1] = heat / 2; // G
            data[index + 2] = heat;   // R
            data[index + 3] = 255;    // A
        }
    }
    return CreateTextureFromData(data.data(), size, size, outSRV);
}
