#include "TransitionService.h"
#include "imgui.h"

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void TransitionService::addPortal(const Portal& portal) {
    portals_.push_back(portal);
}

void TransitionService::addFloorElevator(const FloorElevator& elevator) {
    elevators_.push_back(elevator);
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

        if (portal.requiresE) {
            promptVisible_ = true;
            promptHint_    = "Presiona E";
            if (IsKeyPressed(KEY_E)) {
                beginFadeIn(targetScene, spawnPos);
            }
        } else {
            beginFadeIn(targetScene, spawnPos);
        }
        return; // process at most one trigger per frame
    }

    // -----------------------------------------------------------------------
    // Floor elevator triggers
    // -----------------------------------------------------------------------
    for (int i = 0; i < static_cast<int>(elevators_.size()); ++i) {
        const auto& elev = elevators_[i];
        if (elev.scene != currentScene) continue;
        if (!CheckCollisionRecs(playerCollider, elev.triggerRect)) continue;

        promptVisible_     = true;
        promptHint_        = "Presiona E para cambiar de piso";
        if (IsKeyPressed(KEY_E)) {
            showFloorMenu_     = true;
            activeElevatorIdx_ = i;
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
    bool open = true;

    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
               ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Seleccionar piso", &open,
                 ImGuiWindowFlags_NoResize   |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoMove     |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::Text("Selecciona el piso al que deseas ir:");
    ImGui::Separator();

    for (const auto& entry : elev.floors) {
        const char* lbl = entry.label.empty() ? entry.scene.c_str()
                                              : entry.label.c_str();
        if (ImGui::Button(lbl, ImVec2(-1.0f, 0.0f))) {
            beginFadeIn(entry.scene, entry.spawnPos);
            showFloorMenu_     = false;
            activeElevatorIdx_ = -1;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Cancelar", ImVec2(-1.0f, 0.0f))) {
        showFloorMenu_     = false;
        activeElevatorIdx_ = -1;
    }

    ImGui::End();

    // Also handle the X close button
    if (!open) {
        showFloorMenu_     = false;
        activeElevatorIdx_ = -1;
    }
}

void TransitionService::drawFadeOverlay(int screenWidth, int screenHeight) const {
    if (fadeAlpha_ <= 0.0f) return;
    const auto alpha = static_cast<unsigned char>(fadeAlpha_ * 255.0f);
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, alpha});
}
