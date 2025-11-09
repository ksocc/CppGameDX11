#include "GameEngine.h"

bool GameEngine::CreatePlayerMesh()
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    playerParts.clear();

    const float headSize = 0.5f;     
    const float bodyWidth = 0.45f;   
    const float bodyHeight = 0.8f;    
    const float armWidth = 0.18f;      
    const float armHeight = 0.8f;     
    const float legWidth = 0.22f;      
    const float legHeight = 0.8f;     

    auto addPart = [&](const std::string& name, XMFLOAT3 size, XMFLOAT3 pivot)
        {
            uint32_t indexStart = (uint32_t)indices.size();
            CreateCube(vertices, indices, 0.0f, 0.0f, 0.0f, size.x, size.y, size.z);

            PlayerPart part;
            part.name = name;
            part.size = size;
            part.pivot = pivot;
            part.indexStart = indexStart;
            part.indexCount = (uint32_t)indices.size() - indexStart;

            playerParts.push_back(part);
        };
    addPart("body",
        { bodyWidth, bodyHeight, bodyWidth / 2 },
        { 0.0f, legHeight + bodyHeight * 0.5f, 0.0f });

    addPart("head",
        { headSize, headSize, headSize },
        { 0.0f, legHeight + bodyHeight + headSize * 0.5f, 0.0f });

    addPart("armL",
        { armWidth, armHeight, armWidth },
        { -(bodyWidth * 0.5f + armWidth * 0.5f), legHeight + bodyHeight * 0.5f, 0.0f });

    addPart("armR",
        { armWidth, armHeight, armWidth },
        { +(bodyWidth * 0.5f + armWidth * 0.5f), legHeight + bodyHeight * 0.5f, 0.0f });

    addPart("legL",
        { legWidth, legHeight, legWidth },
        { -bodyWidth * 0.25f, legHeight * 0.5f, 0.0f });

    addPart("legR",
        { legWidth, legHeight, legWidth },
        { +bodyWidth * 0.25f, legHeight * 0.5f, 0.0f });

    playerIndexCount = (UINT)indices.size();

    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = (UINT)(vertices.size() * sizeof(Vertex));
    vb.Usage = D3D11_USAGE_IMMUTABLE;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbd{ vertices.data() };
    if (FAILED(pDevice->CreateBuffer(&vb, &vbd, &pPlayerVB))) return false;

    D3D11_BUFFER_DESC ib{};
    ib.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
    ib.Usage = D3D11_USAGE_IMMUTABLE;
    ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibd{ indices.data() };
    if (FAILED(pDevice->CreateBuffer(&ib, &ibd, &pPlayerIB))) return false;

    return true;
}
void GameEngine::RenderPlayer(const XMFLOAT3& pos, float rotationY, bool isSelf, float headPitch)
{
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    D3D11_MAPPED_SUBRESOURCE mapped3D{};
    ConstantBuffer* cb3D = nullptr;

    pContext->IASetVertexBuffers(0, 1, &pPlayerVB, &stride, &offset);
    pContext->IASetIndexBuffer(pPlayerIB, DXGI_FORMAT_R16_UINT, 0);

    XMMATRIX baseWorld = XMMatrixRotationY(rotationY) * XMMatrixTranslation(pos.x, pos.y, pos.z);

    float speed = XMVectorGetX(XMVector3Length(XMLoadFloat3(&playerVelocity)));
    if (speed > 0.01f) walkTime += 0.1f; else walkTime = 0;

    float swing = sinf(walkTime * 5.0f) * 0.4f;

    int partIndex = 0;
    int partVertexCount = playerIndexCount / playerParts.size();

    for (auto& part : playerParts) {
        // first 
        if (isSelf && !thirdPerson && part.name == "head") {
            partIndex++;
            continue;
        }

        XMMATRIX local = XMMatrixIdentity();

        if (isSelf) { // loacl player
            if (part.name == "head") {
                local = XMMatrixRotationX(headPitch);
            }
            else {
                if (part.name == "armL")
                    local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                    XMMatrixRotationX(swing) *
                    XMMatrixTranslation(0, part.size.y / 2, 0);
                else if (part.name == "armR")
                    local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                    XMMatrixRotationX(-swing) *
                    XMMatrixTranslation(0, part.size.y / 2, 0);
                else if (part.name == "legL")
                    local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                    XMMatrixRotationX(-swing) *
                    XMMatrixTranslation(0, part.size.y / 2, 0);
                else if (part.name == "legR")
                    local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                    XMMatrixRotationX(swing) *
                    XMMatrixTranslation(0, part.size.y / 2, 0);
            }
        }
        else { // server gamer
            if (part.name == "armL")
                local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                XMMatrixRotationX(swing) *
                XMMatrixTranslation(0, part.size.y / 2, 0);
            else if (part.name == "armR")
                local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                XMMatrixRotationX(-swing) *
                XMMatrixTranslation(0, part.size.y / 2, 0);
            else if (part.name == "legL")
                local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                XMMatrixRotationX(-swing) *
                XMMatrixTranslation(0, part.size.y / 2, 0);
            else if (part.name == "legR")
                local = XMMatrixTranslation(0, -part.size.y / 2, 0) *
                XMMatrixRotationX(swing) *
                XMMatrixTranslation(0, part.size.y / 2, 0);
        }

        XMMATRIX world =
            local *
            XMMatrixTranslation(part.pivot.x, part.pivot.y, part.pivot.z) *
            baseWorld;

        pContext->Map(pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped3D);
        cb3D = reinterpret_cast<ConstantBuffer*>(mapped3D.pData);
        cb3D->world = XMMatrixTranspose(world);
        cb3D->view = XMMatrixTranspose(viewMatrix);
        cb3D->projection = XMMatrixTranspose(projectionMatrix);
        cb3D->cameraPos = cameraPosition;
        cb3D->lightDir = XMFLOAT3(0.4f, -0.7f, 0.3f);
        cb3D->lightColor = XMFLOAT3(1.0f, 0.98f, 0.95f);
        cb3D->ambient = XMFLOAT3(0.2f, 0.2f, 0.25f);
        pContext->Unmap(pCB, 0);

        pContext->PSSetShaderResources(0, 1, &pPlayerSRV);
        pContext->DrawIndexed(partVertexCount, partIndex * partVertexCount, 0);

        partIndex++;
    }
}