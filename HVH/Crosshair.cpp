#include "GameEngine.h"

bool GameEngine::CreateCrosshairMesh()
{
    std::vector<Vertex2D> vertices = {
        { XMFLOAT2(-10.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },  // l
        { XMFLOAT2(10.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },  
        
        { XMFLOAT2(0.0f, -10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT2(0.0f, 10.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }    // down
    };

    std::vector<uint16_t> indices = {
        0, 1, 
        2, 3  
    };

    crosshairIndexCount = static_cast<UINT>(indices.size());

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(Vertex2D) * static_cast<UINT>(vertices.size());
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbData, &pCrosshairVB)))
        return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(uint16_t) * crosshairIndexCount;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibData, &pCrosshairIB)))
        return false;

    return true;
}