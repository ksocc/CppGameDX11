#include "GameEngine.h"

void GameEngine::OnResize(UINT width, UINT height)
{
    if (!pSwapChain || width == 0 || height == 0) return;

    pContext->OMSetRenderTargets(0, nullptr, nullptr);
    SafeRelease(&pRTV);
    SafeRelease(&pDSV);

    HRESULT hr = pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return;

    ID3D11Texture2D* pBack = nullptr;
    hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBack);
    if (FAILED(hr) || !pBack)
    {
        MessageBox(nullptr, L"Failed to GetBuffer CODE:18", L"OnResize", MB_OK | MB_ICONERROR);
        return;
    }

    hr = pDevice->CreateRenderTargetView(pBack, nullptr, &pRTV);
    pBack->Release(); //SafeRelease(&pBack);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to CreateRenderTargetView CODE:26", L"OnResize", MB_OK | MB_ICONERROR);
        return;
    }

    //Depth Stencil View
    ID3D11Texture2D* pDepth = nullptr;
    D3D11_TEXTURE2D_DESC dtd{};
    dtd.Width = width;
    dtd.Height = height;
    dtd.MipLevels = 1;
    dtd.ArraySize = 1;
    dtd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dtd.SampleDesc.Count = 1;
    dtd.Usage = D3D11_USAGE_DEFAULT;
    dtd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dtd.CPUAccessFlags = 0;
    dtd.MiscFlags = 0;

    hr = pDevice->CreateTexture2D(&dtd, nullptr, &pDepth);
    if (FAILED(hr) || !pDepth)
    {
        MessageBox(nullptr, L"Failed to CreateTexture2D CODE:47", L"OnResize", MB_OK | MB_ICONERROR);
        return;
    }

    hr = pDevice->CreateDepthStencilView(pDepth, nullptr, &pDSV);
    pDepth->Release(); //SafeRelease(&pDepth);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create DepthStencilView CODE:55", L"OnResize", MB_OK | MB_ICONERROR);
        return;
    }

    pContext->OMSetRenderTargets(1, &pRTV, pDSV);

    D3D11_VIEWPORT vp{};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    pContext->RSSetViewports(1, &vp);

    projectionMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (float)height, 0.01f, 500.f);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(pContext->Map(p2DCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        XMMATRIX orthoMatrix = XMMatrixOrthographicLH((float)width, (float)height, 0.0f, 1.0f);
        XMMATRIX* cb = reinterpret_cast<XMMATRIX*>(mapped.pData);
        *cb = XMMatrixTranspose(orthoMatrix);
        pContext->Unmap(p2DCB, 0);
    }
}