#include "GameEngine.h"

bool GameEngine::CreateHealthBarMesh()
{
    struct HealthVertex {
        XMFLOAT2 position;
        XMFLOAT4 color;
    };

    healthBarWidth = 120.0f;   
    healthBarHeight = 20.0f;   
    healthBarYOffset = -500.0f;  
    healthBarXOffset = 20.0f; 

    const int vertexCount = 4;
    HealthVertex vertices[vertexCount] = {
        {{-healthBarWidth + healthBarXOffset, healthBarYOffset - healthBarHeight / 2}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-left (red)
        {{-healthBarWidth + healthBarXOffset, healthBarYOffset + healthBarHeight / 2}, {1.0f, 0.5f, 0.0f, 1.0f}},  // Top-left (orange)
        {{ healthBarWidth + healthBarXOffset, healthBarYOffset + healthBarHeight / 2}, {1.0f, 1.0f, 0.0f, 1.0f}},  // Top-right (yellow)
        {{ healthBarWidth + healthBarXOffset, healthBarYOffset - healthBarHeight / 2}, {0.0f, 1.0f, 0.0f, 1.0f}}   // Bottom-right (green)
    };

    uint16_t indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };

    if (pHealthVB) {
        pHealthVB->Release();
        pHealthVB = nullptr;
    }
    if (pHealthIB) {
        pHealthIB->Release();
        pHealthIB = nullptr;
    }

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vbd = {};
    vbd.pSysMem = vertices;
    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbd, &pHealthVB))) {
        return false;
    }

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;
    ibDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA ibd = {};
    ibd.pSysMem = indices;
    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibd, &pHealthIB))) {
        pHealthVB->Release();
        pHealthVB = nullptr;
        return false;
    }

    healthIndexCount = _countof(indices);

    return true;
}

void GameEngine::UpdateHealthBar(float healthPercentage)
{
#undef max
#undef min
    healthPercentage = std::max(0.0f, std::min(1.0f, healthPercentage));

    XMMATRIX scaleMatrix = XMMatrixScaling(healthPercentage, 1.0f, 1.0f);

    XMMATRIX worldMatrix = scaleMatrix * XMMatrixTranslation(healthBarXOffset, healthBarYOffset, 0.0f);
    XMStoreFloat4x4(&healthBarWorldMatrix, worldMatrix);
}
