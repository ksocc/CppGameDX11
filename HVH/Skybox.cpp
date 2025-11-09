#include "Skybox.h"
#include <vector>
#include <d3dcompiler.h>
#include "SafeRelease.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

Skybox::Skybox() {

}

Skybox::~Skybox() 
{ 
    Cleanup(); 
}

bool Skybox::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    pDevice = device;
    pContext = context;

    if (!CreateCubeMesh()) return false;
    if (!CreateShaders()) return false;
    if (!CreateTexture()) return false;

    return true;
}

void Skybox::SetHorizonColor(float r, float g, float b)
{
    horizonColor = XMFLOAT3(r, g, b);
    UpdateSkyTexture();
}

void Skybox::SetZenithColor(float r, float g, float b)
{
    zenithColor = XMFLOAT3(r, g, b);
    UpdateSkyTexture();
}

void Skybox::SetHazeColor(float r, float g, float b)
{
    hazeColor = XMFLOAT3(r, g, b);
    UpdateSkyTexture();
}

void Skybox::UpdateSkyTexture()
{
    SafeRelease(&pTextureSRV);
    CreateTexture();
}

bool Skybox::CreateCubeMesh()
{
    struct SkyboxVertex {
        XMFLOAT3 position;
        SkyboxVertex(float x, float y, float z) : position(x, y, z) {}
    };

    const float scale = 10.0f;
    SkyboxVertex vertices[] = {
        // Front face
        {-1.0f * scale, -1.0f * scale, -1.0f * scale}, {-1.0f * scale,  1.0f * scale, -1.0f * scale},
        { 1.0f * scale,  1.0f * scale, -1.0f * scale}, { 1.0f * scale, -1.0f * scale, -1.0f * scale},
        // Back face
        {-1.0f * scale, -1.0f * scale,  1.0f * scale}, { 1.0f * scale, -1.0f * scale,  1.0f * scale},
        { 1.0f * scale,  1.0f * scale,  1.0f * scale}, {-1.0f * scale,  1.0f * scale,  1.0f * scale},
        // Top face
        {-1.0f * scale,  1.0f * scale, -1.0f * scale}, {-1.0f * scale,  1.0f * scale,  1.0f * scale},
        { 1.0f * scale,  1.0f * scale,  1.0f * scale}, { 1.0f * scale,  1.0f * scale, -1.0f * scale},
        // Bottom face
        {-1.0f * scale, -1.0f * scale, -1.0f * scale}, { 1.0f * scale, -1.0f * scale, -1.0f * scale},
        { 1.0f * scale, -1.0f * scale,  1.0f * scale}, {-1.0f * scale, -1.0f * scale,  1.0f * scale},
        // Left face
        {-1.0f * scale, -1.0f * scale,  1.0f * scale}, {-1.0f * scale,  1.0f * scale,  1.0f * scale},
        {-1.0f * scale,  1.0f * scale, -1.0f * scale}, {-1.0f * scale, -1.0f * scale, -1.0f * scale},
        // Right face
        { 1.0f * scale, -1.0f * scale, -1.0f * scale}, { 1.0f * scale,  1.0f * scale, -1.0f * scale},
        { 1.0f * scale,  1.0f * scale,  1.0f * scale}, { 1.0f * scale, -1.0f * scale,  1.0f * scale}
    };

    uint16_t indices[] = {
        0,1,2, 0,2,3,       // Front
        4,5,6, 4,6,7,       // Back
        8,9,10, 8,10,11,    // Top
        12,13,14, 12,14,15, // Bottom
        16,17,18, 16,18,19, // Left
        20,21,22, 20,22,23  // Right
    };

    indexCount = 36;

    // Vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    HRESULT hr = pDevice->CreateBuffer(&vbDesc, &vbData, &pVertexBuffer);
    if (FAILED(hr)) return false;

    // Index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    hr = pDevice->CreateBuffer(&ibDesc, &ibData, &pIndexBuffer);
    return SUCCEEDED(hr);
}

bool Skybox::CreateShaders()
{
    const char* vsSource = R"(
    struct VS_INPUT { float3 position : POSITION; };
    struct PS_INPUT { float4 position : SV_POSITION; float3 texCoord : TEXCOORD0; };
    
    cbuffer ConstantBuffer : register(b0) {
        matrix ViewProjection;
    }
    
    PS_INPUT main(VS_INPUT input) {
        PS_INPUT output;
        
        output.position = mul(float4(input.position, 1.0), ViewProjection);
        output.texCoord = input.position;
        
        return output;
    }
)";

    const char* psSource = R"(
    TextureCube skyboxTexture : register(t0);
    SamplerState skyboxSampler : register(s0);
    
    float4 main(float4 position : SV_POSITION, float3 texCoord : TEXCOORD0) : SV_Target {
        return skyboxTexture.Sample(skyboxSampler, texCoord);
    }
)";

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        SafeRelease(&errorBlob);
        return false;
    }

    hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &pVertexShader);
    if (FAILED(hr)) { SafeRelease(&vsBlob); return false; }

    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        SafeRelease(&errorBlob);
        SafeRelease(&vsBlob);
        return false;
    }

    hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pPixelShader);
    if (FAILED(hr)) { SafeRelease(&vsBlob); SafeRelease(&psBlob); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = pDevice->CreateInputLayout(layout, 1, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &pInputLayout);

    SafeRelease(&vsBlob);
    SafeRelease(&psBlob);
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(XMMATRIX);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = pDevice->CreateBuffer(&cbDesc, nullptr, &pConstantBuffer);
    if (FAILED(hr)) return false;

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = true;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    hr = pDevice->CreateDepthStencilState(&depthDesc, &pDepthState);
    if (FAILED(hr)) return false;

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_FRONT;
    rasterDesc.FrontCounterClockwise = false;
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.SlopeScaledDepthBias = 0.0f;
    rasterDesc.DepthClipEnable = true;
    rasterDesc.ScissorEnable = false;
    rasterDesc.MultisampleEnable = false;
    rasterDesc.AntialiasedLineEnable = false;

    hr = pDevice->CreateRasterizerState(&rasterDesc, &pRasterState);

    return SUCCEEDED(hr);
}

bool Skybox::CreateTexture()
{
    using namespace DirectX;
    const int TEXTURE_SIZE = 512;
    std::vector<BYTE> textureData(TEXTURE_SIZE * TEXTURE_SIZE * 6 * 4);

    auto TexelToDir = [&](int face, float u, float v) -> XMFLOAT3 {
        switch (face)
        {
        case 0: return XMFLOAT3(1.0f, v, -u);   // +X
        case 1: return XMFLOAT3(-1.0f, v, u);   // -X
        case 2: return XMFLOAT3(u, 1.0f, -v);   // +Y
        case 3: return XMFLOAT3(u, -1.0f, v);   // -Y
        case 4: return XMFLOAT3(u, v, 1.0f);    // +Z
        case 5: return XMFLOAT3(-u, v, -1.0f);  // -Z
        default: return XMFLOAT3(0, 0, 1);
        }
        };

    for (int face = 0; face < 6; ++face)
    {
        for (int y = 0; y < TEXTURE_SIZE; ++y)
        {
            for (int x = 0; x < TEXTURE_SIZE; ++x)
            {
                int idx = ((face * TEXTURE_SIZE + y) * TEXTURE_SIZE + x) * 4;
                float u = (x + 0.5f) / TEXTURE_SIZE * 2.0f - 1.0f;
                float v = (y + 0.5f) / TEXTURE_SIZE * 2.0f - 1.0f;
                v = -v;

                XMFLOAT3 dir = TexelToDir(face, u, v);
                XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&dir));
                XMFLOAT3 n;
                XMStoreFloat3(&n, d);

                float t = n.y * 0.5f + 0.5f;
                float horizonBlend = powf(t, 0.6f);

                float r = horizonColor.x * (1.0f - horizonBlend) + zenithColor.x * horizonBlend;
                float g = horizonColor.y * (1.0f - horizonBlend) + zenithColor.y * horizonBlend;
                float b = horizonColor.z * (1.0f - horizonBlend) + zenithColor.z * horizonBlend;

                float haze = expf(-fabsf(n.y) * 3.5f);
                r += haze * hazeColor.x;
                g += haze * hazeColor.y;
                b += haze * hazeColor.z;

                float subtle = 0.5f + 0.5f * sinf((n.x * 10.0f + n.z * 7.0f) * 0.3f);
                r *= 0.97f + 0.03f * subtle;
                g *= 0.97f + 0.03f * subtle;
                b *= 0.97f + 0.03f * subtle;

                auto clampByte = [](float v)->BYTE {
#undef max
#undef min
                    return static_cast<BYTE>(std::max(0.0f, std::min(255.0f, v)));
                    };

                textureData[idx + 0] = clampByte(r * 255.0f);
                textureData[idx + 1] = clampByte(g * 255.0f);
                textureData[idx + 2] = clampByte(b * 255.0f);
                textureData[idx + 3] = 255;
            }
        }
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = TEXTURE_SIZE;
    texDesc.Height = TEXTURE_SIZE;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 6;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA subData[6];
    for (int i = 0; i < 6; ++i)
    {
        subData[i].pSysMem = &textureData[i * TEXTURE_SIZE * TEXTURE_SIZE * 4];
        subData[i].SysMemPitch = TEXTURE_SIZE * 4;
        subData[i].SysMemSlicePitch = 0;
    }

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = pDevice->CreateTexture2D(&texDesc, subData, &pTexture);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    hr = pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pTextureSRV);
    SafeRelease(&pTexture);
    if (FAILED(hr)) return false;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = pDevice->CreateSamplerState(&sampDesc, &pSampler);
    return SUCCEEDED(hr);
}

void Skybox::Render(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& projection)
{
    ID3D11RasterizerState* oldRasterState = nullptr;
    context->RSGetState(&oldRasterState);
    if (pRasterState) {
        context->RSSetState(pRasterState);
    }
    ID3D11DepthStencilState* oldDepthState = nullptr;
    UINT oldStencilRef;
    context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);
    context->OMSetDepthStencilState(pDepthState, 0);
    XMMATRIX viewNoTranslation = view;
    viewNoTranslation.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMMATRIX viewProj = XMMatrixMultiply(viewNoTranslation, projection);
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        context->OMSetDepthStencilState(oldDepthState, oldStencilRef);
        if (oldRasterState) context->RSSetState(oldRasterState);
        SafeRelease(&oldDepthState);
        SafeRelease(&oldRasterState);
        return;
    }

    XMMATRIX* data = (XMMATRIX*)mapped.pData;
    *data = XMMatrixTranspose(viewProj);
    context->Unmap(pConstantBuffer, 0);

    UINT stride = sizeof(XMFLOAT3);
    UINT offset = 0;

    context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(pInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(pVertexShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    context->PSSetShader(pPixelShader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &pTextureSRV);
    context->PSSetSamplers(0, 1, &pSampler);

    context->DrawIndexed(indexCount, 0, 0);

    context->OMSetDepthStencilState(oldDepthState, oldStencilRef);
    SafeRelease(&oldDepthState);

    if (oldRasterState) {
        context->RSSetState(oldRasterState);
        SafeRelease(&oldRasterState);
    }
}

void Skybox::Cleanup()
{
    SafeRelease(&pVertexBuffer);
    SafeRelease(&pIndexBuffer);
    SafeRelease(&pVertexShader);
    SafeRelease(&pPixelShader);
    SafeRelease(&pInputLayout);
    SafeRelease(&pTextureSRV);
    SafeRelease(&pSampler);
    SafeRelease(&pConstantBuffer);
    SafeRelease(&pDepthState);
    SafeRelease(&pRasterState);
}


bool HorizonPlane::Initialize(ID3D11Device* device, float _maxRadius, float _fadeDistance)
{
    pDevice = device;
    maxRadius = _maxRadius;
    fadeDistance = _fadeDistance;

    fogColorNear = { 1.0f, 1.0f, 1.0f };
    fogColorFar = { 1.0f, 1.0f, 1.0f };

    int stacks = 32;  
    int slices = 32; 
    float radius = maxRadius + fadeDistance;

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    for (int i = 0; i <= stacks; ++i)
    {
        float phi = DirectX::XM_PI * i / stacks;  

        for (int j = 0; j <= slices; ++j)
        {
            float theta = 2.0f * DirectX::XM_PI * j / slices; 

            Vertex vertex;
            vertex.pos.x = radius * sinf(phi) * cosf(theta);
            vertex.pos.y = radius * cosf(phi);
            vertex.pos.z = radius * sinf(phi) * sinf(theta);
            vertex.texCoord.x = (float)j / slices;
            vertex.texCoord.y = (float)i / stacks;

            vertices.push_back(vertex);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
        }
    }

    indexCount = (UINT)indices.size();

    // Vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(vertices.size() * sizeof(Vertex));
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = { vertices.data() };
    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbData, &pVertexBuffer))) return false;

    // Index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = { indices.data() };
    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibData, &pIndexBuffer))) return false;

    // Vertex shader
    const char* vsSource = R"(
cbuffer CB : register(b0)
{
    matrix ViewProj;
    float3 CameraPos;
    float Time;
    float3 FogColorNear;
    float3 FogColorFar;
    float padding;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    
    output.worldPos = input.pos + CameraPos;
    output.pos = mul(float4(output.worldPos, 1.0f), ViewProj);
    
    return output;
}
    )";

    // Pixel shader 
    const char* psSource = R"(
cbuffer CB : register(b0)
{
    matrix ViewProj;
    float3 CameraPos;
    float Time;
    float3 FogColorNear;
    float3 FogColorFar;
    float padding;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

float hash(float3 p)
{
    return frac(sin(dot(p, float3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    float3 u = f * f * (3.0 - 2.0 * f);

    return lerp(
        lerp(
            lerp(hash(i + float3(0,0,0)), hash(i + float3(1,0,0)), u.x),
            lerp(hash(i + float3(0,1,0)), hash(i + float3(1,1,0)), u.x), u.y),
        lerp(
            lerp(hash(i + float3(0,0,1)), hash(i + float3(1,0,1)), u.x),
            lerp(hash(i + float3(0,1,1)), hash(i + float3(1,1,1)), u.x), u.y), u.z);
}

float fbm(float3 p)
{
    float v = 0.0;
    float a = 0.5;
    float f = 1.0;
    for(int i = 0; i < 5; i++)
    {
        v += a * noise(p * f);
        f *= 2.0;
        a *= 0.5;
    }
    return saturate(v);
}

float4 main(PS_INPUT input) : SV_Target
{
    float3 localPos = input.worldPos - CameraPos;
    float dist = length(localPos);

    float maxRadius = 2.0;
    float fadeDistance = 2.0;
    float fogFactor = saturate((dist - maxRadius) / fadeDistance);

    float3 uv = localPos * 0.5 + float3(Time * 0.01, Time * 0.05, 0);
    float cloudNoise = fbm(uv);

    fogFactor *= smoothstep(0.3, 1.0, cloudNoise);
    fogFactor *= 1;

   float3 fogColor = FogColorNear;

    return float4(fogColor, fogFactor);
}
    )";

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &pVertexShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }

    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        vsBlob->Release();
        return false;
    }

    hr = pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pPixelShader);
    if (FAILED(hr)) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = pDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);

    vsBlob->Release();
    psBlob->Release();
    if (errorBlob) errorBlob->Release();

    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(HorizonCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pDevice->CreateBuffer(&cbDesc, nullptr, &pConstantBuffer)))
        return false;

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (FAILED(pDevice->CreateBlendState(&blendDesc, &pBlendState)))
        return false;

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    if (FAILED(pDevice->CreateRasterizerState(&rasterDesc, &pRasterState)))
        return false;

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(pDevice->CreateDepthStencilState(&depthDesc, &pDepthState)))
        return false;

    return true;
}

void HorizonPlane::Render(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj, const XMFLOAT3& camPos, float time)
{

    XMMATRIX wvp = view * proj;

    HorizonCB cbData;
    cbData.ViewProj = XMMatrixTranspose(wvp);
    cbData.CameraPos = camPos;
    cbData.Time = time;
    cbData.FogColorNear = fogColorNear;
    cbData.FogColorFar = fogColorFar;
    cbData.Padding = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &cbData, sizeof(cbData));
        context->Unmap(pConstantBuffer, 0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetInputLayout(pInputLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(pVertexShader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    context->PSSetShader(pPixelShader, nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &pConstantBuffer);

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->OMSetBlendState(pBlendState, blendFactor, 0xffffffff);
    context->OMSetDepthStencilState(pDepthState, 0);
    context->RSSetState(pRasterState);
    context->DrawIndexed(indexCount, 0, 0);
    context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
    context->OMSetDepthStencilState(nullptr, 0);
    context->RSSetState(nullptr);
}

void HorizonPlane::Cleanup() {
    SafeRelease(&pVertexBuffer);
    SafeRelease(&pIndexBuffer);
    SafeRelease(&pVertexShader);
    SafeRelease(&pPixelShader);
    SafeRelease(&pInputLayout);
    SafeRelease(&pConstantBuffer);
    SafeRelease(&pBlendState);
    SafeRelease(&pRasterState);
    SafeRelease(&pDepthState);
}