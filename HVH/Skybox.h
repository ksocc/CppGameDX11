#pragma once
#define WIN32_LEAN_AND_MEAN
#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

#define M_PI 3.14159265358

using namespace DirectX;

class Skybox
{
public:
    Skybox();
    ~Skybox();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Render(ID3D11DeviceContext* context, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
    void Cleanup();

    void SetHorizonColor(float r, float g, float b);
    void SetZenithColor(float r, float g, float b);
    void SetHazeColor(float r, float g, float b);
    void UpdateSkyTexture();

private:
    bool CreateCubeMesh();
    bool CreateShaders();
    bool CreateTexture();

    float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    float clamp(float value, float min, float max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pIndexBuffer = nullptr;
    ID3D11VertexShader* pVertexShader = nullptr;
    ID3D11PixelShader* pPixelShader = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    ID3D11ShaderResourceView* pTextureSRV = nullptr;
    ID3D11SamplerState* pSampler = nullptr;
    ID3D11Buffer* pConstantBuffer = nullptr;
    ID3D11DepthStencilState* pDepthState = nullptr;
    ID3D11RasterizerState* pRasterState = nullptr;

    UINT indexCount = 0;

    // color param sky
    DirectX::XMFLOAT3 horizonColor = { 0.7f, 0.8f, 0.9f };
    DirectX::XMFLOAT3 zenithColor = { 0.1f, 0.2f, 0.4f };
    DirectX::XMFLOAT3 hazeColor = { 0.4f, 0.3f, 0.2f };
};


class HorizonPlane {
public:
    HorizonPlane() {}
    ~HorizonPlane() { Cleanup(); }

    bool Initialize(ID3D11Device* device, float _maxRadius, float _fadeDistance);
    void Render(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj, const XMFLOAT3& camPos, float time);
    void Cleanup();

    void SetFogColorNear(float r, float g, float b) {
        fogColorNear = XMFLOAT3(r, g, b);
    }
    void SetFogColorFar(float r, float g, float b) {
        fogColorFar = XMFLOAT3(r, g, b);
    }

private:
    struct Vertex {
        XMFLOAT3 pos;
        XMFLOAT2 texCoord;
    };

    struct HorizonCB {
        XMMATRIX ViewProj;
        XMFLOAT3 CameraPos;
        float Time;
        XMFLOAT3 FogColorNear; 
        XMFLOAT3 FogColorFar;
        float Padding;
    };

    ID3D11Device* pDevice = nullptr;

    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pIndexBuffer = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    ID3D11VertexShader* pVertexShader = nullptr;
    ID3D11PixelShader* pPixelShader = nullptr;
    ID3D11Buffer* pConstantBuffer = nullptr;
    ID3D11BlendState* pBlendState = nullptr;
    ID3D11RasterizerState* pRasterState = nullptr;
    ID3D11DepthStencilState* pDepthState = nullptr;

    UINT indexCount = 0;

    float maxRadius = 2.0f;
    float fadeDistance = 2.0f;

    XMFLOAT3 fogColorNear = { 0.95f, 0.95f, 0.98f }; 
    XMFLOAT3 fogColorFar = { 0.9f, 0.92f, 1.0f };
};