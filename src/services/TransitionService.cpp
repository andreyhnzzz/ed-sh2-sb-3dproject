#include "TransitionService.h"
#include "RuntimeBlockerService.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void TransitionService::addPortal(const Portal& portal) {
    portals_.push_back(portal);
}

void TransitionService::addUniPortal(const UniPortal& portal) {
    uniPortals_.push_back(portal);
}

void TransitionService::addFloorElevator(const FloorElevator& elevator) {
    elevators_.push_back(elevator);
}

void TransitionService::setBlockerService(const RuntimeBlockerService* blockerService) {
    blockerService_ = blockerService;
}

// ---------------------------------------------------------------------------
// Update (fade state machine + trigger detection)
// ---------------------------------------------------------------------------

void TransitionService::update(const Rectangle& playerCollider,
                                const std::string& currentScene,
                                float dt) {
    // -----------------------------------------------------------------------
    // Advance fade state machine first
    // -----------------------------------------------------------------------
    if (phase_ == Phase::FADING_IN) {
        fadeAlpha_ += kFadeRate * dt;
        if (fadeAlpha_ >= 1.0f) {
            fadeAlpha_   = 1.0f;
            swapPending_ = true;    // signal main to swap the scene
            // Stay in FADING_IN until notifySwapDone() is called
        }
        return; // don't process new triggers while fading
    }

    if (phase_ == Phase::FADING_OUT) {
        fadeAlpha_ -= kFadeRate * dt;
        if (fadeAlpha_ <= 0.0f) {
            fadeAlpha_ = 0.0f;
            phase_     = Phase::IDLE;
        }
        return;
    }

    // -----------------------------------------------------------------------
    // When the floor menu is open, skip trigger detection
    // -----------------------------------------------------------------------
    if (showFloorMenu_) return;

    promptVisible_ = false;
    promptHint_.clear();

    if (triggerLockUntilExit_) {
        if (isCollidingWithSceneTrigger(playerCollider, currentScene)) {
            return;
        }
        triggerLockUntilExit_ = false;
    }

    // -----------------------------------------------------------------------
    // Portal triggers
    // -----------------------------------------------------------------------
    for (const auto& portal : portals_) {
        const bool inA = (portal.sceneA == currentScene) &&
                         CheckCollisionRecs(playerCollider, portal.triggerA);
        const bool inB = (portal.sceneB == currentScene) &&
                         CheckCollisionRecs(playerCollider, portal.triggerB);

        if (!inA && !inB) continue;

        const std::string& targetScene = inA ? portal.sceneB : portal.sceneA;
        const Vector2&     spawnPos    = inA ? portal.spawnInB : portal.spawnInA;
        if (!isDestinationAccessible(targetScene)) continue;

        if (portal.requiresE) {
            promptVisible_ = true;
            promptHint_    = "Press E";
            if (IsKeyPressed(KEY_E)) {
                beginFadeIn(targetScene, spawnPos);
            }
        } else {
            beginFadeIn(targetScene, spawnPos);
        }
        return; // process at most one trigger per frame
    }

    // -----------------------------------------------------------------------
    // Floor elevator triggers (priority over uni-portals when close/overlapping)
    // -----------------------------------------------------------------------
    for (int i = 0; i < static_cast<int>(elevators_.size()); ++i) {
        const auto& elev = elevators_[i];
        if (elev.scene != currentScene) continue;
        if (!CheckCollisionRecs(playerCollider, elev.triggerRect)) continue;

        bool hasAccessibleDestination = false;
        for (const auto& floor : elev.floors) {
            if (floor.scene == currentScene) continue;
            if (isDestinationAccessible(floor.scene)) {
                hasAccessibleDestination = true;
                break;
            }
        }
        if (!hasAccessibleDestination) continue;

        promptVisible_ = true;
        const std::string label = elev.interactionLabel.empty() ? "access" : elev.interactionLabel;
        promptHint_    = "Press E to use " + label;
        if (IsKeyPressed(KEY_E)) {
            showFloorMenu_     = true;
            activeElevatorIdx_ = i;
            selectedFloorIdx_  = 0;
            floorMenuConfirmArmed_ = false;
        }
        return;
    }

    // -----------------------------------------------------------------------
    // Uni-directional portal triggers (loaded from TMJ)
    // -----------------------------------------------------------------------
    for (const auto& portal : uniPortals_) {
        if (portal.scene != currentScene) continue;
        if (!CheckCollisionRecs(playerCollider, portal.triggerRect)) continue;
        if (!isDestinationAccessible(portal.targetScene)) continue;

        if (portal.requiresE) {
            promptVisible_ = true;
            promptHint_    = "Press E";
            if (IsKeyPressed(KEY_E)) {
                beginFadeIn(portal.targetScene, portal.spawnPos);
            }
        } else {
            beginFadeIn(portal.targetScene, portal.spawnPos);
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// Scene-swap handshake
// ---------------------------------------------------------------------------

void TransitionService::notifySwapDone() {
    swapPending_ = false;
    phase_       = Phase::FADING_OUT;
    triggerLockUntilExit_ = true;
}

bool TransitionService::isFading() const {
    return phase_ != Phase::IDLE;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void TransitionService::beginFadeIn(const std::string& targetScene,
                                     const Vector2&     spawnPos) {
    if (phase_ != Phase::IDLE) return;
    pending_     = {targetScene, spawnPos};
    phase_       = Phase::FADING_IN;
    fadeAlpha_   = 0.0f;
    swapPending_ = false;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void TransitionService::drawFloorMenu() {
    if (!showFloorMenu_ || activeElevatorIdx_ < 0 ||
        activeElevatorIdx_ >= static_cast<int>(elevators_.size())) {
        return;
    }

    const auto& elev = elevators_[activeElevatorIdx_];
    if (elev.floors.empty()) {
        showFloorMenu_     = false;
        activeElevatorIdx_ = -1;
        selectedFloorIdx_  = 0;
        floorMenuConfirmArmed_ = false;
        return;
    }

    std::vector<int> accessibleFloorIndices;
    accessibleFloorIndices.reserve(elev.floors.size());
    for (int i = 0; i < static_cast<int>(elev.floors.size()); ++i) {
        const auto& floor = elev.floors[i];
        if (floor.scene == elev.scene) continue;
        if (!isDestinationAccessible(floor.scene)) continue;
        accessibleFloorIndices.push_back(i);
    }

    if (accessibleFloorIndices.empty()) {
        showFloorMenu_     = false;
        activeElevatorIdx_ = -1;
        selectedFloorIdx_  = 0;
        floorMenuConfirmArmed_ = false;
        return;
    }

    const int floorCount = static_cast<int>(accessibleFloorIndices.size());
    selectedFloorIdx_ = std::clamp(selectedFloorIdx_, 0, floorCount - 1);

    if (IsKeyPressed(KEY_UP)) {
        selectedFloorIdx_ = (selectedFloorIdx_ - 1 + floorCount) % floorCount;
    } else if (IsKeyPressed(KEY_DOWN)) {
        selectedFloorIdx_ = (selectedFloorIdx_ + 1) % floorCount;
    }

    for (int i = 0; i < std::min(9, floorCount); ++i) {
        if (IsKeyPressed(KEY_ONE + i)) selectedFloorIdx_ = i;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        showFloorMenu_     = false;
        activeElevatorIdx_ = -1;
        selectedFloorIdx_  = 0;
        floorMenuConfirmArmed_ = false;
        return;
    }

    if (!floorMenuConfirmArmed_) {
        if (!IsKeyDown(KEY_E) && !IsKeyDown(KEY_ENTER)) {
            floorMenuConfirmArmed_ = true;
        }
    }

    if (floorMenuConfirmArmed_ && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E))) {
        const auto& entry = elev.floors[accessibleFloorIndices[selectedFloorIdx_]];
        beginFadeIn(entry.scene, entry.spawnPos);
        showFloorMenu_     = false;
        activeElevatorIdx_ = -1;
        selectedFloorIdx_  = 0;
        floorMenuConfirmArmed_ = false;
        return;
    }

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();
    const int panelW = 360;
    const int panelHeaderH = 54;
    const int rowH = 34;
    const int panelH = panelHeaderH + floorCount * rowH + 58;
    const int panelX = (screenW - panelW) / 2;
    const int panelY = (screenH - panelH) / 2;

    DrawRectangle(panelX, panelY, panelW, panelH, Color{10, 14, 24, 235});
    DrawRectangleLinesEx(Rectangle{
        static_cast<float>(panelX),
        static_cast<float>(panelY),
        static_cast<float>(panelW),
        static_cast<float>(panelH)
    }, 2.0f, Color{90, 150, 255, 230});

    DrawText("Select floor", panelX + 16, panelY + 12, 24, RAYWHITE);
    DrawText("Arrows / 1-9 and Enter (Esc cancels)", panelX + 16, panelY + 34, 14, Color{190, 210, 230, 255});

    for (int i = 0; i < floorCount; ++i) {
        const int rowY = panelY + panelHeaderH + i * rowH;
        const bool selected = (i == selectedFloorIdx_);
        const Color bg = selected ? Color{40, 95, 170, 220} : Color{18, 24, 36, 180};
        const Color fg = selected ? WHITE : Color{210, 220, 235, 255};
        DrawRectangle(panelX + 14, rowY, panelW - 28, rowH - 4, bg);

        const auto& entry = elev.floors[accessibleFloorIndices[i]];
        const std::string label = entry.label.empty() ? entry.scene : entry.label;
        const std::string text = std::to_string(i + 1) + ". " + label;
        DrawText(text.c_str(), panelX + 24, rowY + 7, 20, fg);
    }

    DrawText("Enter/E: confirm", panelX + 16, panelY + panelH - 34, 16, Color{175, 235, 180, 255});
    DrawText("Esc: cancel", panelX + panelW - 110, panelY + panelH - 34, 16, Color{240, 190, 190, 255});
}

bool TransitionService::isDestinationAccessible(const std::string& sceneId) const {
    if (!blockerService_) return true;
    return !blockerService_->isNodeBlocked(sceneId);
}

bool TransitionService::isCollidingWithSceneTrigger(const Rectangle& playerCollider,
                                                    const std::string& currentScene) const {
    for (const auto& portal : portals_) {
        const bool inA = (portal.sceneA == currentScene) &&
                         CheckCollisionRecs(playerCollider, portal.triggerA);
        const bool inB = (portal.sceneB == currentScene) &&
                         CheckCollisionRecs(playerCollider, portal.triggerB);
        if (inA || inB) return true;
    }

    for (const auto& portal : uniPortals_) {
        if (portal.scene != currentScene) continue;
        if (CheckCollisionRecs(playerCollider, portal.triggerRect)) return true;
    }

    for (const auto& elev : elevators_) {
        if (elev.scene != currentScene) continue;
        if (CheckCollisionRecs(playerCollider, elev.triggerRect)) return true;
    }

    return false;
}

void TransitionService::drawFadeOverlay(int screenWidth, int screenHeight) const {
    if (fadeAlpha_ <= 0.0f) return;
    const auto alpha = static_cast<unsigned char>(fadeAlpha_ * 255.0f);
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, alpha});
}
