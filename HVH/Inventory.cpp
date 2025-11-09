# include "GameEngine.h"

bool GameEngine::CreateInventoryMesh()
{
    const float slotSize = 50.0f;
    const float spacing = 10.0f;
    const int slotCount = INVENTORY_SLOTS;
    const float selectBorder = 3.0f;

    struct InventoryVertex {
        XMFLOAT2 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

    std::vector<InventoryVertex> slotVertices;
    std::vector<uint16_t> slotIndices;
    std::vector<InventoryVertex> selectVertices;
    std::vector<uint16_t> selectIndices;

    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);

    float totalWidth = slotCount * slotSize + (slotCount - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startY = -screenHeight / 2.0f + slotSize + 20.0f;

    for (int i = 0; i < slotCount; ++i)
    {
        float x = startX + i * (slotSize + spacing);
        float y = startY;

        uint16_t baseIndex = (uint16_t)slotVertices.size();

        slotVertices.push_back({ XMFLOAT2(x, y), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) });
        slotVertices.push_back({ XMFLOAT2(x, y + slotSize), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
        slotVertices.push_back({ XMFLOAT2(x + slotSize, y + slotSize), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) });
        slotVertices.push_back({ XMFLOAT2(x + slotSize, y), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) });

        slotIndices.push_back(baseIndex + 0);
        slotIndices.push_back(baseIndex + 1);
        slotIndices.push_back(baseIndex + 2);
        slotIndices.push_back(baseIndex + 0);
        slotIndices.push_back(baseIndex + 2);
        slotIndices.push_back(baseIndex + 3);
    }

    float selectX = startX + selectedInventorySlot * (slotSize + spacing) - selectBorder;
    float selectY = startY - selectBorder;
    uint16_t selectBaseIndex = (uint16_t)selectVertices.size();

    selectVertices.push_back({ XMFLOAT2(selectX, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX, selectY + slotSize + 2 * selectBorder), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 2 * selectBorder, selectY + slotSize + 2 * selectBorder), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 2 * selectBorder, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });

    selectIndices.push_back(selectBaseIndex + 0);
    selectIndices.push_back(selectBaseIndex + 1);
    selectIndices.push_back(selectBaseIndex + 1);
    selectIndices.push_back(selectBaseIndex + 2);
    selectIndices.push_back(selectBaseIndex + 2);
    selectIndices.push_back(selectBaseIndex + 3);
    selectIndices.push_back(selectBaseIndex + 3);
    selectIndices.push_back(selectBaseIndex + 0);

    inventoryIndexCount = (UINT)slotIndices.size();
    inventorySelectIndexCount = (UINT)selectIndices.size();

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(slotVertices.size() * sizeof(InventoryVertex));
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA vbd = { slotVertices.data(), 0, 0 };
    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbd, &pInventoryVB))) return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(slotIndices.size() * sizeof(uint16_t));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibd = { slotIndices.data(), 0, 0 };
    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibd, &pInventoryIB))) return false;

    vbDesc.ByteWidth = (UINT)(selectVertices.size() * sizeof(InventoryVertex));
    vbd.pSysMem = selectVertices.data();
    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbd, &pInventorySelectVB))) return false;


    ibDesc.ByteWidth = (UINT)(selectIndices.size() * sizeof(uint16_t));
    ibd.pSysMem = selectIndices.data();
    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibd, &pInventorySelectIB))) return false;

    return true;
}

void GameEngine::UpdateInventorySelection()
{
    const float slotSize = 50.0f;
    const float spacing = 10.0f;
    const float selectBorder = 3.0f;

    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);

    float totalWidth = INVENTORY_SLOTS * slotSize + (INVENTORY_SLOTS - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startY = -screenHeight / 2.0f + slotSize + 20.0f;

    float selectX = startX + selectedInventorySlot * (slotSize + spacing) - selectBorder;
    float selectY = startY - selectBorder;

    struct InventoryVertex {
        XMFLOAT2 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

    std::vector<InventoryVertex> selectVertices;
    selectVertices.push_back({ XMFLOAT2(selectX, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX, selectY + slotSize + 2 * selectBorder), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 2 * selectBorder, selectY + slotSize + 2 * selectBorder), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 2 * selectBorder, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(pContext->Map(pInventorySelectVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, selectVertices.data(), selectVertices.size() * sizeof(InventoryVertex));
        pContext->Unmap(pInventorySelectVB, 0);
    }
}

void GameEngine::UpdateCreativeInventorySelection()
{
    const float slotSize = 40.0f;
    const float spacing = 5.0f;
    const int columns = 9;

    struct InventoryVertex {
        XMFLOAT2 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);

    float totalWidth = columns * slotSize + (columns - 1) * spacing;
    float totalHeight = 4 * slotSize + 3 * spacing; // 4 
    float startX = -totalWidth / 2.0f;
    float startY = totalHeight / 2.0f;

    int selectedCol = creativeSelectedSlot % columns;
    int selectedRow = creativeSelectedSlot / columns;
    float selectX = startX + selectedCol * (slotSize + spacing) - 2.0f;
    float selectY = startY - selectedRow * (slotSize + spacing) - 2.0f;

    std::vector<InventoryVertex> selectVertices;
    selectVertices.push_back({ XMFLOAT2(selectX, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX, selectY + slotSize + 4.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 4.0f, selectY + slotSize + 4.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 4.0f, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(pContext->Map(pCreativeSelectVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, selectVertices.data(), selectVertices.size() * sizeof(InventoryVertex));
        pContext->Unmap(pCreativeSelectVB, 0);
    }
}


void GameEngine::OpenInventory()
{
    inventoryOpen = true;
    mouseLocked = false;

    ShowCursor(TRUE);

    if (selectedInventorySlot < inventoryBlocks.size()) {
        auto it = std::find(creativeBlocks.begin(), creativeBlocks.end(), inventoryBlocks[selectedInventorySlot]);
        if (it != creativeBlocks.end()) {
            creativeSelectedSlot = std::distance(creativeBlocks.begin(), it);
            if (creativeSelectedSlot >= (int)creativeBlocks.size()) {
                creativeSelectedSlot = (int)creativeBlocks.size() - 1;
            }
            UpdateCreativeInventorySelection();
        }
    }
    std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
}

void GameEngine::CloseInventory()
{
    inventoryOpen = false;
    mouseLocked = true;
    ShowCursor(FALSE);

    if (creativeSelectedSlot < creativeBlocks.size()) {
        inventoryBlocks[selectedInventorySlot] = creativeBlocks[creativeSelectedSlot];
        UpdateInventorySelection();

        std::lock_guard<std::recursive_mutex> lock(consoleHistoryMutex);
        AddToHistory("Selected: " + creativeBlocks[creativeSelectedSlot]);
    }

    firstMouse = true;
}

ID3D11ShaderResourceView* GameEngine::GetTextureForBlock(const std::string& blockType)
{
    if (blockType == "grass") return pGrassSRV;
    else if (blockType == "stone") return pStoneSRV;
    else if (blockType == "wood") return pWoodSRV;
    else if (blockType == "metal") return pMetalSRV;
    else if (blockType == "brick") return pBrickSRV;
    else if (blockType == "dirt") return pDirtSRV;
    else if (blockType == "water") return pWaterSRV;
    else if (blockType == "lava") return pLavaSRV;
    else return pCubeSRV;
}

void GameEngine::RenderBlockInInventorySlot(int slotIndex, const std::string& blockType)
{
    const float slotSize = 40.0f;
    const float spacing = 5.0f;
    const int columns = 9;
    const int rows = 4;

    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);

    float totalWidth = columns * slotSize + (columns - 1) * spacing;
    float totalHeight = rows * slotSize + (rows - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startY = totalHeight / 2.0f;

    int col = slotIndex % columns;
    int row = slotIndex / columns;
    float x = startX + col * (slotSize + spacing) + 5.0f; 
    float y = startY - row * (slotSize + spacing) - 5.0f;

    struct InventoryVertex {
        XMFLOAT2 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

    std::vector<InventoryVertex> iconVertices;
    std::vector<uint16_t> iconIndices;

    float iconSize = slotSize - 10.0f;

    uint16_t baseIndex = (uint16_t)iconVertices.size();
    iconVertices.push_back({ XMFLOAT2(x, y), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) });
    iconVertices.push_back({ XMFLOAT2(x, y + iconSize), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    iconVertices.push_back({ XMFLOAT2(x + iconSize, y + iconSize), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) });
    iconVertices.push_back({ XMFLOAT2(x + iconSize, y), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) });

    iconIndices.push_back(baseIndex + 0);
    iconIndices.push_back(baseIndex + 1);
    iconIndices.push_back(baseIndex + 2);
    iconIndices.push_back(baseIndex + 0);
    iconIndices.push_back(baseIndex + 2);
    iconIndices.push_back(baseIndex + 3);

    ID3D11Buffer* tempVB = nullptr;
    ID3D11Buffer* tempIB = nullptr;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(iconVertices.size() * sizeof(InventoryVertex));
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbd = { iconVertices.data(), 0, 0 };
    pDevice->CreateBuffer(&vbDesc, &vbd, &tempVB);

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(iconIndices.size() * sizeof(uint16_t));
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibd = { iconIndices.data(), 0, 0 };
    pDevice->CreateBuffer(&ibDesc, &ibd, &tempIB);

    ID3D11ShaderResourceView* texture = GetTextureForBlock(blockType);

    UINT stride2D = sizeof(XMFLOAT2) + sizeof(XMFLOAT4) + sizeof(XMFLOAT2);
    UINT offset2D = 0;
    pContext->IASetVertexBuffers(0, 1, &tempVB, &stride2D, &offset2D);
    pContext->IASetIndexBuffer(tempIB, DXGI_FORMAT_R16_UINT, 0);
    pContext->PSSetShaderResources(0, 1, &texture);
    pContext->DrawIndexed((UINT)iconIndices.size(), 0, 0);

    SafeRelease(&tempVB);
    SafeRelease(&tempIB);
}

void GameEngine::RenderCreativeInventory()
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);
    float height = static_cast<float>(rc.bottom - rc.top);
    XMMATRIX orthoMatrix = XMMatrixOrthographicLH(width, height, 0.0f, 1.0f);

    pContext->IASetInputLayout(p2DInputLayout);
    pContext->VSSetShader(p2DVS, nullptr, 0);
    pContext->PSSetShader(p2DPS, nullptr, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_MAPPED_SUBRESOURCE mapped2D{};
    pContext->Map(p2DCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped2D);
    XMMATRIX* cb2D = reinterpret_cast<XMMATRIX*>(mapped2D.pData);
    *cb2D = XMMatrixTranspose(orthoMatrix);
    pContext->Unmap(p2DCB, 0);
    pContext->VSSetConstantBuffers(0, 1, &p2DCB);

    UINT stride2D = sizeof(XMFLOAT2) + sizeof(XMFLOAT4) + sizeof(XMFLOAT2);
    UINT offset2D = 0;

    pContext->IASetVertexBuffers(0, 1, &pCreativeInventoryVB, &stride2D, &offset2D);
    pContext->IASetIndexBuffer(pCreativeInventoryIB, DXGI_FORMAT_R16_UINT, 0);
    pContext->PSSetShaderResources(0, 1, &pWhiteTextureSRV);
    pContext->DrawIndexed(creativeInventoryIndexCount, 0, 0);

    for (int i = 0; i < min((int)creativeBlocks.size(), CREATIVE_SLOTS_PER_PAGE); ++i) {
        RenderBlockInInventorySlot(i, creativeBlocks[i]);
    }

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    pContext->IASetVertexBuffers(0, 1, &pCreativeSelectVB, &stride2D, &offset2D);
    pContext->IASetIndexBuffer(pCreativeSelectIB, DXGI_FORMAT_R16_UINT, 0);
    pContext->PSSetShaderResources(0, 1, &pWhiteTextureSRV);
    pContext->DrawIndexed(creativeSelectIndexCount, 0, 0);

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
bool GameEngine::CreateCreativeInventoryMesh()
{
    const float slotSize = 40.0f;
    const float spacing = 5.0f;
    const int columns = 9;
    const int rows = 4;
    const float selectBorder = 2.0f;

    struct InventoryVertex {
        XMFLOAT2 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };

    std::vector<InventoryVertex> slotVertices;
    std::vector<uint16_t> slotIndices;
    std::vector<InventoryVertex> selectVertices;
    std::vector<uint16_t> selectIndices;

    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);

    float totalWidth = columns * slotSize + (columns - 1) * spacing;
    float totalHeight = rows * slotSize + (rows - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startY = totalHeight / 2.0f;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            int slotIndex = row * columns + col;
            float x = startX + col * (slotSize + spacing);
            float y = startY - row * (slotSize + spacing);

            uint16_t baseIndex = (uint16_t)slotVertices.size();

            slotVertices.push_back({ XMFLOAT2(x, y), XMFLOAT4(0.2f, 0.2f, 0.2f, 0.8f), XMFLOAT2(0.0f, 1.0f) });
            slotVertices.push_back({ XMFLOAT2(x, y + slotSize), XMFLOAT4(0.2f, 0.2f, 0.2f, 0.8f), XMFLOAT2(0.0f, 0.0f) });
            slotVertices.push_back({ XMFLOAT2(x + slotSize, y + slotSize), XMFLOAT4(0.2f, 0.2f, 0.2f, 0.8f), XMFLOAT2(1.0f, 0.0f) });
            slotVertices.push_back({ XMFLOAT2(x + slotSize, y), XMFLOAT4(0.2f, 0.2f, 0.2f, 0.8f), XMFLOAT2(1.0f, 1.0f) });

            slotIndices.push_back(baseIndex + 0);
            slotIndices.push_back(baseIndex + 1);
            slotIndices.push_back(baseIndex + 2);
            slotIndices.push_back(baseIndex + 0);
            slotIndices.push_back(baseIndex + 2);
            slotIndices.push_back(baseIndex + 3);
        }
    }

    int selectedCol = creativeSelectedSlot % columns;
    int selectedRow = creativeSelectedSlot / columns;
    float selectX = startX + selectedCol * (slotSize + spacing) - selectBorder;
    float selectY = startY - selectedRow * (slotSize + spacing) - selectBorder;

    uint16_t selectBaseIndex = (uint16_t)selectVertices.size();
    selectVertices.push_back({ XMFLOAT2(selectX, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX, selectY + slotSize + 2 * selectBorder), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 2 * selectBorder, selectY + slotSize + 2 * selectBorder), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });
    selectVertices.push_back({ XMFLOAT2(selectX + slotSize + 2 * selectBorder, selectY), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) });

    selectIndices.push_back(selectBaseIndex + 0);
    selectIndices.push_back(selectBaseIndex + 1);
    selectIndices.push_back(selectBaseIndex + 1);
    selectIndices.push_back(selectBaseIndex + 2);
    selectIndices.push_back(selectBaseIndex + 2);
    selectIndices.push_back(selectBaseIndex + 3);
    selectIndices.push_back(selectBaseIndex + 3);
    selectIndices.push_back(selectBaseIndex + 0);

    creativeInventoryIndexCount = (UINT)slotIndices.size();
    creativeSelectIndexCount = (UINT)selectIndices.size();

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(slotVertices.size() * sizeof(InventoryVertex));
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA vbd = { slotVertices.data(), 0, 0 };
    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbd, &pCreativeInventoryVB))) return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(slotIndices.size() * sizeof(uint16_t));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibd = { slotIndices.data(), 0, 0 };
    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibd, &pCreativeInventoryIB))) return false;

    vbDesc.ByteWidth = (UINT)(selectVertices.size() * sizeof(InventoryVertex));
    vbd.pSysMem = selectVertices.data();
    if (FAILED(pDevice->CreateBuffer(&vbDesc, &vbd, &pCreativeSelectVB))) return false;

    ibDesc.ByteWidth = (UINT)(selectIndices.size() * sizeof(uint16_t));
    ibd.pSysMem = selectIndices.data();
    if (FAILED(pDevice->CreateBuffer(&ibDesc, &ibd, &pCreativeSelectIB))) return false;

    return true;
}

bool GameEngine::CreateWhiteTexture()
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint32_t whitePixel = 0xFFFFFFFF; //(RGBA: 255, 255, 255, 255)
    D3D11_SUBRESOURCE_DATA initData = { &whitePixel, sizeof(uint32_t), 0 };

    ID3D11Texture2D* whiteTexture = nullptr;
    HRESULT hr = pDevice->CreateTexture2D(&texDesc, &initData, &whiteTexture);
    if (FAILED(hr)) return false;

    hr = pDevice->CreateShaderResourceView(whiteTexture, nullptr, &pWhiteTextureSRV);
    SafeRelease(&whiteTexture);
    return SUCCEEDED(hr);
}