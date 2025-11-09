#include "GameEngine.h"

bool GameEngine::CreatePlayerTextureSRV(UINT size, ID3D11ShaderResourceView** outSRV)
{
    std::vector<BYTE> data(size * size * 4);
    for (UINT y = 0; y < size; ++y) {
        for (UINT x = 0; x < size; ++x) {
            UINT index = (y * size + x) * 4;
            if (y < size / 4) {
                data[index] = 150;    // B
                data[index + 1] = 200; // G
                data[index + 2] = 255; // R
            }
            else if (y < size / 2) {
                data[index] = 50;     // B
                data[index + 1] = 100; // G
                data[index + 2] = 200; // R
            }
            else {
                data[index] = 30;     // B
                data[index + 1] = 60;  // G
                data[index + 2] = 120; // R
            }
            data[index + 3] = 255;    // A
        }
    }
    return CreateTextureFromData(data.data(), size, size, outSRV);
}