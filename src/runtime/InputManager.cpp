#include "InputManager.h"

#include <raylib.h>

#include <cmath>

InputState InputManager::poll(bool uiActive) const {
    InputState state;
    state.uiActive = uiActive;
    state.toggleInfoMenu = IsKeyPressed(KEY_M);
    state.toggleInterestZones = IsKeyPressed(KEY_TAB);
    state.zoomWheelDelta = uiActive ? 0.0f : GetMouseWheelMove();

    if (!uiActive) {
        if (IsKeyDown(KEY_W)) state.moveY -= 1.0f;
        if (IsKeyDown(KEY_S)) state.moveY += 1.0f;
        if (IsKeyDown(KEY_A)) state.moveX -= 1.0f;
        if (IsKeyDown(KEY_D)) state.moveX += 1.0f;
    }

    if (state.moveX != 0.0f || state.moveY != 0.0f) {
        const float len = std::sqrt(state.moveX * state.moveX + state.moveY * state.moveY);
        if (len > 0.0f) {
            state.moveX /= len;
            state.moveY /= len;
        }
    }

    if (!uiActive && IsKeyDown(KEY_W)) {
        state.facingDirection = 1;
    } else if (!uiActive && IsKeyDown(KEY_S)) {
        state.facingDirection = 3;
    } else if (!uiActive && IsKeyDown(KEY_A)) {
        state.facingDirection = 2;
    } else if (!uiActive && IsKeyDown(KEY_D)) {
        state.facingDirection = 0;
    }

    state.sprinting = !uiActive &&
        (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    return state;
}
