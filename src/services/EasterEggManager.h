#pragma once

#include <raylib.h>

#include <string>
#include <vector>

class EasterEggManager {
public:
    struct TeleportRequest {
        std::string sceneName;
        Vector2 spawnPos{0.0f, 0.0f};
    };

    void update(float dt);

    bool isActivated() const { return activated_; }
    bool isScreamerActive() const { return screamerActive_; }
    bool shouldCloseApplication() const { return closeRequested_; }

    bool consumeActivationEvent();
    bool consumePendingTeleport(TeleportRequest& outTeleport);

    bool isPlayerInScreamerZone(const Vector2& playerPos) const;
    void triggerScreamer(const std::string& screamerPath);
    void drawScreamerOverlay(int screenWidth, int screenHeight) const;
    void reset();

private:
    static constexpr int kRequiredSequences = 3;
    static constexpr float kKeyTimeout = 1.5f;
    static constexpr float kScreamerDuration = 1.6f;
    static constexpr float kScreamerMinX = 240.0f;
    static constexpr float kScreamerMaxX = 800.0f;
    static constexpr float kScreamerMinY = 210.0f;
    static constexpr float kScreamerMaxY = 310.0f;

    bool checkForKeyPress(char key) const;
    void queueTeleport(const std::string& sceneName, const Vector2& spawnPos);
    bool bufferEndsWithJeff() const;

    std::vector<char> keyBuffer_{};
    int jeffSequenceCount_{0};
    float keyTimer_{0.0f};
    bool activated_{false};
    bool activationEventPending_{false};
    bool teleportPending_{false};
    TeleportRequest pendingTeleport_{};
    bool screamerTriggered_{false};
    bool screamerActive_{false};
    float screamerTimer_{0.0f};
    bool closeRequested_{false};
    std::string screamerPath_{};
};
