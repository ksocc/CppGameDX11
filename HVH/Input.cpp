#include "GameEngine.h"

bool GameEngine::IsKeySpecial(UINT key)
{
    switch (key) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case VK_CAPITAL:
    case VK_NUMLOCK:
    case VK_SCROLL:
        return true;
    default:
        return false;
    }
}

void GameEngine::HandleInput(float dt)
{
    if (consoleActive) {
        HandleConsoleInput(dt);
        return;
    }

    if (inventoryOpen) {
        HandleInventoryInput();
        return;
    }

    if (keyStates['E'] && !keyProcessed['E']) {
        OpenInventory();
        keyProcessed['E'] = true;
        return;
    }

    if (keyStates[VK_OEM_3] && !keyProcessed[VK_OEM_3]) {
        consoleActive = true;
        consoleInput.clear();
        keyProcessed[VK_OEM_3] = true;
        return;
    }

    if (keyStates['F'] && !keyProcessed['F']) {
        thirdPerson = !thirdPerson;
        keyProcessed['F'] = true;
        //return;
    }

    if (gameMode == BUILD_MODE) {
        for (int key = '1'; key <= '9'; key++) {
            if (keyStates[key] && !keyProcessed[key]) {
                int slot = key - '1';
                if (slot < INVENTORY_SLOTS) {
                    selectedInventorySlot = slot;
                    UpdateInventorySelection();
                    keyProcessed[key] = true;
                }
            }
        }
    }

    XMVECTOR forward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    XMVECTOR right = XMVectorSet(1.f, 0.f, 0.f, 0.f);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
    forward = XMVector3TransformCoord(forward, R);
    right = XMVector3TransformCoord(right, R);

    wishDir = XMFLOAT3(0.f, 0.f, 0.f);

    if (keyStates['W']) {
        wishDir.x += XMVectorGetX(forward);
        wishDir.z += XMVectorGetZ(forward);
    }
    if (keyStates['S']) {
        wishDir.x -= XMVectorGetX(forward);
        wishDir.z -= XMVectorGetZ(forward);
    }
    if (keyStates['A']) {
        wishDir.x -= XMVectorGetX(right);
        wishDir.z -= XMVectorGetZ(right);
    }
    if (keyStates['D']) {
        wishDir.x += XMVectorGetX(right);
        wishDir.z += XMVectorGetZ(right);
    }

    currentSpeed = keyStates[VK_SHIFT] ? runSpeed : walkSpeed;

    float wishLen = sqrtf(wishDir.x * wishDir.x + wishDir.z * wishDir.z);
    if (wishLen > 0.1f) {
        wishDir.x /= wishLen;
        wishDir.z /= wishLen;
    }

    if (keyStates[VK_SPACE] && isGrounded && !flyMode && !keyProcessed[VK_SPACE]) {
        velocity.y = jumpForce;
        isGrounded = false;
        //keyProcessed[VK_SPACE] = true;
    }

    if (flyMode) {
        velocity.y = 0.f;
        XMFLOAT3 moveDir = { 0.f, 0.f, 0.f };

        if (keyStates['W']) {
            moveDir.x += XMVectorGetX(forward);
            moveDir.z += XMVectorGetZ(forward);
        }
        if (keyStates['S']) {
            moveDir.x -= XMVectorGetX(forward);
            moveDir.z -= XMVectorGetZ(forward);
        }
        if (keyStates['A']) {
            moveDir.x -= XMVectorGetX(right);
            moveDir.z -= XMVectorGetZ(right);
        }
        if (keyStates['D']) {
            moveDir.x += XMVectorGetX(right);
            moveDir.z += XMVectorGetZ(right);
        }

        if (keyStates[VK_SPACE]) {
            moveDir.y += 1.0f;
        }
        if (keyStates[VK_CONTROL]) {
            moveDir.y -= 1.0f;
        }

        float moveLen = sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
        if (moveLen > 0.1f) {
            moveDir.x /= moveLen;
            moveDir.y /= moveLen;
            moveDir.z /= moveLen;

            playerPos.x += moveDir.x * currentSpeed * dt;
            playerPos.y += moveDir.y * currentSpeed * dt;
            playerPos.z += moveDir.z * currentSpeed * dt;

            if (!thirdPerson) {
                cameraPosition = { playerPos.x, playerPos.y + 1.6f, playerPos.z };
            }
        }

        UpdateCamera();
        return;
    }

    Physics(dt);
    UpdateCamera();

    if (gameMode == BUILD_MODE && (mouseStates[0] || mouseStates[2])) {
        HandleBuilding();
    }
}

void GameEngine::ResetKeyProcessing()
{
    for (int i = 0; i < 256; i++) {
        if (!keyStates[i]) {
            keyProcessed[i] = false;
        }
    }
}

void GameEngine::HandleConsoleInput(float dt)
{
    if (keyStates[VK_RETURN] && !keyProcessed[VK_RETURN]) {
        ProcessCommand(consoleInput);
        consoleInput.clear();
        keyProcessed[VK_RETURN] = true;
    }

    if (keyStates[VK_ESCAPE] && !keyProcessed[VK_ESCAPE]) {
        consoleActive = false;
        consoleInput.clear();
        keyProcessed[VK_ESCAPE] = true;
    }

    if (keyStates[VK_BACK] && !keyProcessed[VK_BACK]) {
        if (!consoleInput.empty()) {
            consoleInput.pop_back();
        }
        keyProcessed[VK_BACK] = true;
    }

    if (keyStates[VK_UP] && !keyProcessed[VK_UP]) {
        if (historyIndex > 0) {
            historyIndex--;
            if (historyIndex < consoleHistory.size()) {
                consoleInput = consoleHistory[historyIndex];
                if (consoleInput.substr(0, 2) == "> ") {
                    consoleInput = consoleInput.substr(2);
                }
            }
        }
        keyProcessed[VK_UP] = true;
    }
    else if (keyStates[VK_DOWN] && !keyProcessed[VK_DOWN]) {
        if (historyIndex < consoleHistory.size() - 1) {
            historyIndex++;
            consoleInput = consoleHistory[historyIndex];
            if (consoleInput.substr(0, 2) == "> ") {
                consoleInput = consoleInput.substr(2);
            }
        }
        else {
            consoleInput.clear();
            historyIndex = consoleHistory.size();
        }
        keyProcessed[VK_DOWN] = true;
    }
}

void GameEngine::HandleInventoryInput()
{
    const int columns = 9;

    if (keyStates[VK_LEFT] && !keyProcessed[VK_LEFT]) {
        creativeSelectedSlot--;
        if (creativeSelectedSlot < 0) creativeSelectedSlot = min((int)creativeBlocks.size() - 1, CREATIVE_SLOTS_PER_PAGE - 1);
        keyProcessed[VK_LEFT] = true;
        UpdateCreativeInventorySelection();
    }
    else if (keyStates[VK_RIGHT] && !keyProcessed[VK_RIGHT]) {
        creativeSelectedSlot++;
        if (creativeSelectedSlot >= min((int)creativeBlocks.size(), CREATIVE_SLOTS_PER_PAGE)) creativeSelectedSlot = 0;
        keyProcessed[VK_RIGHT] = true;
        UpdateCreativeInventorySelection();
    }
    else if (keyStates[VK_UP] && !keyProcessed[VK_UP]) {
        creativeSelectedSlot -= columns;
        if (creativeSelectedSlot < 0) creativeSelectedSlot += min((int)creativeBlocks.size(), CREATIVE_SLOTS_PER_PAGE);
        keyProcessed[VK_UP] = true;
        UpdateCreativeInventorySelection();
    }
    else if (keyStates[VK_DOWN] && !keyProcessed[VK_DOWN]) {
        creativeSelectedSlot += columns;
        if (creativeSelectedSlot >= min((int)creativeBlocks.size(), CREATIVE_SLOTS_PER_PAGE)) creativeSelectedSlot -= min((int)creativeBlocks.size(), CREATIVE_SLOTS_PER_PAGE);
        keyProcessed[VK_DOWN] = true;
        UpdateCreativeInventorySelection();
    }
    else if (keyStates[VK_RETURN] && !keyProcessed[VK_RETURN]) {
        if (creativeSelectedSlot < creativeBlocks.size()) {
            inventoryBlocks[selectedInventorySlot] = creativeBlocks[creativeSelectedSlot];
            UpdateInventorySelection();
        }
        keyProcessed[VK_RETURN] = true;
    }
    else if (keyStates[VK_ESCAPE] && !keyProcessed[VK_ESCAPE]) {
        CloseInventory();
        keyProcessed[VK_ESCAPE] = true;
    }
    else {
        for (int i = '1'; i <= '9'; i++) {
            if (keyStates[i] && !keyProcessed[i]) {
                int slot = i - '1';
                if (slot < INVENTORY_SLOTS) {
                    selectedInventorySlot = slot;
                    UpdateInventorySelection();
                    keyProcessed[i] = true;
                }
            }
        }
    }
}

void GameEngine::OnKeyDown(UINT key)
{
    if (key < 256) keyStates[key] = true;

    if (consoleActive) {
        return;
    }

    if (key == VK_OEM_3) {
        consoleActive = true;
        consoleInput.clear();
        return;
    }

    if (inventoryOpen) {
        return;
    }

    if (key >= '1' && key <= '9' && gameMode == BUILD_MODE) {
        int slot = key - '1';
        if (slot < INVENTORY_SLOTS) {
            selectedInventorySlot = slot;
            UpdateInventorySelection();
        }
    }
}

void GameEngine::OnKeyUp(UINT key)
{
    if (key < 256) {
        keyStates[key] = false;
        keyProcessed[key] = false;
    }
}

void GameEngine::OnChar(UINT ch)
{
    if (!consoleActive) return;

    if (ch >= 32 && ch <= 126) {
        consoleInput += static_cast<char>(ch);
    }
}

void GameEngine::OnMouseDown(UINT button)
{
    if (button == 0) mouseStates[0] = true;
    else if (button == 1) mouseStates[1] = true;
    else if (button == 2) mouseStates[2] = true;

    if (consoleActive) return;

    if (inventoryOpen) {
        HandleInventoryMouseClick(button);
        return;
    }

    if (gameMode == BUILD_MODE) {
        HandleBuildModeMouseClick(button);
    }
    else if (gameMode == HVH_MODE) {
        HandleHvHModeMouseClick(button);
    }
}

void GameEngine::OnMouseUp(UINT button)
{
    if (button == 0) mouseStates[0] = false;
    else if (button == 1) mouseStates[1] = false;
    else if (button == 2) mouseStates[2] = false;
}

void GameEngine::OnMouseWheel(short delta)
{
    if (consoleActive) return;

    if (inventoryOpen) {
        if (delta > 0) {
            creativeCurrentPage--;
            if (creativeCurrentPage < 0) creativeCurrentPage = 0;
        }
        else {
            creativeCurrentPage++;
            int maxPages = (creativeBlocks.size() + CREATIVE_SLOTS_PER_PAGE - 1) / CREATIVE_SLOTS_PER_PAGE;
            if (creativeCurrentPage >= maxPages) creativeCurrentPage = maxPages - 1;
        }
        return;
    }

    if (gameMode == HVH_MODE) return;

    if (delta > 0) {
        selectedInventorySlot = (selectedInventorySlot - 1 + INVENTORY_SLOTS) % INVENTORY_SLOTS;
    }
    else {
        selectedInventorySlot = (selectedInventorySlot + 1) % INVENTORY_SLOTS;
    }

    UpdateInventorySelection();
}

void GameEngine::OnMouseMove(int x, int y, bool captureMouse)
{
    if (inventoryOpen) {
        HandleInventoryMouse(x, y);
        return;
    }

    if (consoleActive) return;

    if (firstMouse) {
        lastMousePos = { x, y };
        firstMouse = false;
        return;
    }

    int dx = x - lastMousePos.x;
    int dy = y - lastMousePos.y;
    lastMousePos = { x, y };

    cameraRotation.y += dx * mouseSensitivity;
    cameraRotation.x += dy * mouseSensitivity;

    const float lim = XM_PIDIV2 - 0.1f;
    if (cameraRotation.x > lim) cameraRotation.x = lim;
    if (cameraRotation.x < -lim) cameraRotation.x = -lim;

    if (captureMouse && mouseLocked) {
        RECT rc; GetClientRect(hWnd, &rc);
        int cx = (rc.right - rc.left) / 2;
        int cy = (rc.bottom - rc.top) / 2;
        POINT center = { cx, cy };
        ClientToScreen(hWnd, &center);
        SetCursorPos(center.x, center.y);
        lastMousePos = { cx, cy };
    }
}

void GameEngine::HandleInventoryMouseClick(UINT button)
{
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(hWnd, &mousePos);

    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);
    float closeButtonSize = 30.0f;
    float closeButtonX = screenWidth - closeButtonSize - 10.0f;
    float closeButtonY = 10.0f;

    if (mousePos.x >= closeButtonX && mousePos.x <= closeButtonX + closeButtonSize &&
        mousePos.y >= closeButtonY && mousePos.y <= closeButtonY + closeButtonSize) {
        if (button == 0) {
            CloseInventory();
            return; 
        }
    }
    const float slotSize = 40.0f;
    const float spacing = 5.0f;
    const int columns = 9;
    const int rows = 4;
    float totalWidth = columns * slotSize + (columns - 1) * spacing;
    float totalHeight = rows * slotSize + (rows - 1) * spacing;
    float startX = (screenWidth - totalWidth) / 2.0f;
    float startY = (screenHeight - totalHeight) / 2.0f;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            int slotIndex = row * columns + col;
            float slotX = startX + col * (slotSize + spacing);
            float slotY = startY + row * (slotSize + spacing);

            if (mousePos.x >= slotX && mousePos.x <= slotX + slotSize &&
                mousePos.y >= slotY && mousePos.y <= slotY + slotSize) {

                if (button == 0) {
                    creativeSelectedSlot = slotIndex;
                    UpdateCreativeInventorySelection();

                    if (creativeSelectedSlot < creativeBlocks.size()) {
                        inventoryBlocks[selectedInventorySlot] = creativeBlocks[creativeSelectedSlot];
                        UpdateInventorySelection();
                    }
                }
                else if (button == 2) { 
                    if (slotIndex < creativeBlocks.size()) {
                        inventoryBlocks[selectedInventorySlot] = creativeBlocks[slotIndex];
                        UpdateInventorySelection();
                    }
                }
                return; 
            }
        }
    }
}

void GameEngine::HandleBuildModeMouseClick(UINT button)
{
    if (button == 0 && breakCooldown <= 0) {
        mouseStates[0] = true;
        XMVECTOR defaultForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
        XMVECTOR lookDir = XMVector3TransformCoord(defaultForward, R);
        XMFLOAT3 rayDir;
        XMStoreFloat3(&rayDir, XMVector3Normalize(lookDir));
        XMFLOAT3 rayOrigin = cameraPosition;
        float closestT = buildReach;
        int hitIndex = -1;

        for (int i = 0; i < (int)mapObjects.size(); ++i) {
            const auto& obj = mapObjects[i];
            AABB box;
            box.min = { obj.position.x - obj.scale.x, obj.position.y - obj.scale.y, obj.position.z - obj.scale.z };
            box.max = { obj.position.x + obj.scale.x, obj.position.y + obj.scale.y, obj.position.z + obj.scale.z };

            float t;
            if (RayIntersectsAABB(rayOrigin, rayDir, box, t) && t < closestT) {
                closestT = t;
                hitIndex = i;
            }
        }

        if (hitIndex != -1) {
            const auto& obj = mapObjects[hitIndex];
            if (isMultiplayer) {
                std::string removeData = "BLOCK:REMOVE|" +
                    std::to_string(obj.position.x) + "," +
                    std::to_string(obj.position.y) + "," +
                    std::to_string(obj.position.z);
                if (networkManager.IsServer()) {
                    networkManager.BroadcastData(removeData);
                }
                else {
                    networkManager.SendData(removeData);
                }
            }
            mapObjects.erase(mapObjects.begin() + hitIndex);
            breakCooldown = BREAK_DELAY;
        }
    }
    else if (button == 2 && placeCooldown <= 0) {
        mouseStates[2] = true;
        XMVECTOR defaultForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
        XMVECTOR lookDir = XMVector3TransformCoord(defaultForward, R);
        XMFLOAT3 rayDir;
        XMStoreFloat3(&rayDir, XMVector3Normalize(lookDir));
        XMFLOAT3 rayOrigin = cameraPosition;

        float closestT = buildReach;
        int hitIndex = -1;
        XMFLOAT3 hitNormal = { 0.f, 0.f, 0.f };
        XMFLOAT3 hitPoint = { 0.0f, 0.0f, 0.0f };

        for (int i = 0; i < (int)mapObjects.size(); ++i) {
            const auto& obj = mapObjects[i];
            AABB box;
            box.min = { obj.position.x - obj.scale.x, obj.position.y - obj.scale.y, obj.position.z - obj.scale.z };
            box.max = { obj.position.x + obj.scale.x, obj.position.y + obj.scale.y, obj.position.z + obj.scale.z };

            float t;
            if (RayIntersectsAABB(rayOrigin, rayDir, box, t) && t < closestT) {
                closestT = t;
                hitIndex = i;
                hitPoint.x = rayOrigin.x + t * rayDir.x;
                hitPoint.y = rayOrigin.y + t * rayDir.y;
                hitPoint.z = rayOrigin.z + t * rayDir.z;

                float eps = 0.01f;
                if (abs(hitPoint.x - box.min.x) < eps) hitNormal = { -1.f, 0.f, 0.f };
                else if (abs(hitPoint.x - box.max.x) < eps) hitNormal = { 1.f, 0.f, 0.f };
                else if (abs(hitPoint.y - box.min.y) < eps) hitNormal = { 0.f, -1.f, 0.f };
                else if (abs(hitPoint.y - box.max.y) < eps) hitNormal = { 0.f, 1.f, 0.f };
                else if (abs(hitPoint.z - box.min.z) < eps) hitNormal = { 0.f, 0.f, -1.f };
                else if (abs(hitPoint.z - box.max.z) < eps) hitNormal = { 0.f, 0.f, 1.f };
            }
        }

        bool hitFloor = false;
        float tFloor = -rayOrigin.y / rayDir.y;
        if (tFloor > 0.0f && tFloor < buildReach && rayDir.y < 0.0f) {
            hitFloor = true;
            XMFLOAT3 hitPointFloor;
            hitPointFloor.x = rayOrigin.x + tFloor * rayDir.x;
            hitPointFloor.y = 0.0f;
            hitPointFloor.z = rayOrigin.z + tFloor * rayDir.z;
            XMFLOAT3 newPos = GetPlacementPosition(hitPointFloor, XMFLOAT3(0.f, 1.f, 0.f));
            newPos.y = 1.0f;
            if (!IsBlockAtPosition(newPos)) {
                PlaceBlock(newPos);
                placeCooldown = PLACE_DELAY;
            }
        }
        else if (hitIndex != -1) {
            XMFLOAT3 newPos = GetPlacementPosition(hitPoint, hitNormal);
            if (!IsBlockAtPosition(newPos)) {
                PlaceBlock(newPos);
                placeCooldown = PLACE_DELAY;
            }
        }
    }
    else if (button == 1) {
        XMVECTOR defaultForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
        XMVECTOR lookDir = XMVector3TransformCoord(defaultForward, R);
        XMFLOAT3 rayDir;
        XMStoreFloat3(&rayDir, XMVector3Normalize(lookDir));
        XMFLOAT3 rayOrigin = cameraPosition;

        float closestT = buildReach;
        int hitIndex = -1;

        for (int i = 0; i < (int)mapObjects.size(); ++i) {
            const auto& obj = mapObjects[i];
            AABB box;
            box.min = { obj.position.x - obj.scale.x, obj.position.y - obj.scale.y, obj.position.z - obj.scale.z };
            box.max = { obj.position.x + obj.scale.x, obj.position.y + obj.scale.y, obj.position.z + obj.scale.z };

            float t;
            if (RayIntersectsAABB(rayOrigin, rayDir, box, t) && t < closestT) {
                closestT = t;
                hitIndex = i;
            }
        }

        if (hitIndex != -1) {
            const auto& obj = mapObjects[hitIndex];
            inventoryBlocks[selectedInventorySlot] = obj.type;
            UpdateInventorySelection();
        }
    }
}

void GameEngine::HandleHvHModeMouseClick(UINT button)
{
    if (button == 0) {
        XMVECTOR defaultForward = XMVectorSet(0.f, 0.f, 1.f, 0.f);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(cameraRotation.x, cameraRotation.y, 0.f);
        XMVECTOR lookDir = XMVector3TransformCoord(defaultForward, R);
        XMFLOAT3 rayDir;
        XMStoreFloat3(&rayDir, XMVector3Normalize(lookDir));
        XMFLOAT3 rayOrigin = cameraPosition;

        float closestT = 10.0f;
        int hitPlayerId = -1;

        for (const auto& pair : networkPlayers) {
            const NetworkPlayer& player = pair.second;
            XMFLOAT3 toPlayer = {
                player.position.x - rayOrigin.x,
                player.position.y + 1.0f - rayOrigin.y,
                player.position.z - rayOrigin.z
            };

            float distance = sqrtf(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);
            float dotProduct = (rayDir.x * toPlayer.x + rayDir.y * toPlayer.y + rayDir.z * toPlayer.z) / distance;

            if (distance < closestT && dotProduct > 0.9f) {
                closestT = distance;
                hitPlayerId = pair.first;
            }
        }

        if (hitPlayerId != -1) {
            if (isMultiplayer) {
                std::string hitData = "HIT:" + std::to_string(hitPlayerId);
                networkManager.SendData(hitData);
            }
        }
    }
}

void GameEngine::HandleInventoryMouse(int x, int y)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    float screenWidth = static_cast<float>(rc.right - rc.left);
    float screenHeight = static_cast<float>(rc.bottom - rc.top);
    float mouseX = (2.0f * x / screenWidth) - 1.0f;
    float mouseY = 1.0f - (2.0f * y / screenHeight);
    const float slotSize = 40.0f / screenWidth * 2.0f;
    const float spacing = 5.0f / screenWidth * 2.0f;
    const int columns = 9;
    const int rows = 4;
    float totalWidth = columns * slotSize + (columns - 1) * spacing;
    float startX = -totalWidth / 2.0f;
    float startY = 0.2f;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < columns; ++col) {
            int slotIndex = row * columns + col;
            float slotX = startX + col * (slotSize + spacing);
            float slotY = startY - row * (slotSize + spacing);

            if (mouseX >= slotX && mouseX <= slotX + slotSize &&
                mouseY >= slotY && mouseY <= slotY + slotSize) {

                creativeSelectedSlot = slotIndex;
                UpdateCreativeInventorySelection();

                if (mouseStates[0]) {
                    if (slotIndex < creativeBlocks.size()) {
                        inventoryBlocks[selectedInventorySlot] = creativeBlocks[slotIndex];
                        UpdateInventorySelection();
                    }
                    mouseStates[0] = false;
                }
                return;
            }
        }
    }
}