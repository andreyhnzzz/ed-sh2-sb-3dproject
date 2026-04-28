#include "services/EasterEggManager.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>

void EasterEggManager::update(float dt) {
    if (screamerActive_) {
        screamerTimer_ = std::max(0.0f, screamerTimer_ - dt);
        if (screamerTimer_ <= 0.0f) {
            screamerActive_ = false;
            closeRequested_ = true;
        }
        return;
    }

    if (activated_) return;

    if (!keyBuffer_.empty()) {
        keyTimer_ -= dt;
        if (keyTimer_ <= 0.0f) {
            keyBuffer_.clear();
            jeffSequenceCount_ = 0;
            keyTimer_ = 0.0f;
        }
    }

    constexpr std::array<char, 4> kJeffKeys{'J', 'E', 'F', 'F'};
    for (char key : kJeffKeys) {
        if (!checkForKeyPress(key)) continue;

        keyBuffer_.push_back(key);
        keyTimer_ = kKeyTimeout;

        if (bufferEndsWithJeff()) {
            ++jeffSequenceCount_;
            keyBuffer_.clear();
            std::cout << "[EasterEgg] JEFF sequence detected: "
                      << jeffSequenceCount_ << "/" << kRequiredSequences << "\n";
            if (jeffSequenceCount_ >= kRequiredSequences) {
                activated_ = true;
                activationEventPending_ = true;
                queueTeleport("easter_egg", Vector2{510.0f, 375.0f});
                std::cout << "[EasterEgg] Easter egg activated.\n";
            }
        } else if (keyBuffer_.size() > kJeffKeys.size()) {
            keyBuffer_.erase(keyBuffer_.begin());
        }
        break;
    }
}

bool EasterEggManager::consumeActivationEvent() {
    if (!activationEventPending_) return false;
    activationEventPending_ = false;
    return true;
}

bool EasterEggManager::consumePendingTeleport(TeleportRequest& outTeleport) {
    if (!teleportPending_) return false;
    teleportPending_ = false;
    outTeleport = pendingTeleport_;
    return true;
}

bool EasterEggManager::isPlayerInScreamerZone(const Vector2& playerPos) const {
    return playerPos.x >= kScreamerMinX && playerPos.x <= kScreamerMaxX &&
           playerPos.y >= kScreamerMinY && playerPos.y <= kScreamerMaxY;
}

void EasterEggManager::triggerScreamer(const std::string& screamerPath) {
    if (screamerTriggered_ || screamerActive_) return;

    screamerTriggered_ = true;
    screamerActive_ = true;
    screamerTimer_ = kScreamerDuration;
    screamerPath_ = screamerPath;

    if (!screamerPath.empty() && !std::filesystem::exists(screamerPath)) {
        std::cout << "[EasterEgg] Screamer asset not found at: " << screamerPath << "\n";
    } else if (!screamerPath.empty()) {
        std::cout << "[EasterEgg] Screamer triggered with asset: " << screamerPath << "\n";
    } else {
        std::cout << "[EasterEgg] Screamer triggered without asset path.\n";
    }
}

void EasterEggManager::drawScreamerOverlay(int screenWidth, int screenHeight) const {
    if (!screamerActive_) return;

    const float normalized = std::clamp(screamerTimer_ / kScreamerDuration, 0.0f, 1.0f);
    const float pulse = 0.5f + 0.5f * std::sin((1.0f - normalized) * 42.0f);
    const unsigned char alpha = static_cast<unsigned char>(160 + pulse * 80.0f);

    DrawRectangle(0, 0, screenWidth, screenHeight, Color{120, 0, 0, alpha});
    DrawRectangleLinesEx(Rectangle{18.0f, 18.0f,
                                   static_cast<float>(screenWidth - 36),
                                   static_cast<float>(screenHeight - 36)},
                         6.0f, Color{255, 230, 230, 240});

    const char* headline = "ITS ME";
    const int fontSize = std::max(48, screenWidth / 12);
    const int textWidth = MeasureText(headline, fontSize);
    DrawText(headline, (screenWidth - textWidth) / 2, screenHeight / 2 - fontSize,
             fontSize, RAYWHITE);
}

void EasterEggManager::reset() {
    keyBuffer_.clear();
    jeffSequenceCount_ = 0;
    keyTimer_ = 0.0f;
    activated_ = false;
    activationEventPending_ = false;
    teleportPending_ = false;
    pendingTeleport_ = {};
    screamerTriggered_ = false;
    screamerActive_ = false;
    screamerTimer_ = 0.0f;
    closeRequested_ = false;
    screamerPath_.clear();
}

bool EasterEggManager::checkForKeyPress(char key) const {
    switch (key) {
        case 'J': return IsKeyPressed(KEY_J);
        case 'E': return IsKeyPressed(KEY_E);
        case 'F': return IsKeyPressed(KEY_F);
        default: return false;
    }
}

void EasterEggManager::queueTeleport(const std::string& sceneName, const Vector2& spawnPos) {
    pendingTeleport_ = {sceneName, spawnPos};
    teleportPending_ = true;
}

bool EasterEggManager::bufferEndsWithJeff() const {
    constexpr std::array<char, 4> kJeff{'J', 'E', 'F', 'F'};
    if (keyBuffer_.size() < kJeff.size()) return false;
    const size_t start = keyBuffer_.size() - kJeff.size();
    for (size_t i = 0; i < kJeff.size(); ++i) {
        if (keyBuffer_[start + i] != kJeff[i]) return false;
    }
    return true;
}
