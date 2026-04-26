#pragma once

#include "InputState.h"
#include "core/runtime/SceneRuntimeTypes.h"

#include <raylib.h>

#include <string>

class GameController {
public:
    struct Config {
        float baseSpeed{150.0f};
        float sprintMultiplier{1.8f};
        float renderScale{1.6f};
        float minZoom{1.2f};
        float maxZoom{4.0f};
        float defaultZoom{2.2f};
    };

    explicit GameController(Config config = {});

    void update(float dt, const InputState& input, MapRenderData& mapData);
    void applyZoom(float wheelDelta);
    void clampCameraToMap(const MapRenderData& mapData, int screenWidth, int screenHeight);
    void drawPlayer() const;
    void resetZoom();
    void unloadPlayerSprites();

    void setPlayerPosition(const Vector2& pos);
    void setCameraOffset(const Vector2& offset);
    void loadPlayerSprites(const std::string& idlePath, const std::string& walkPath);

    const Camera2D& getCamera() const;
    const SpriteAnim& getPlayerAnim() const;
    Vector2 getPlayerPos() const;
    bool isMoving() const;
    bool hadCollisionThisFrame() const;

private:
    Config config_;
    Vector2 playerPos_{0.0f, 0.0f};
    Camera2D camera_{};
    SpriteAnim playerAnim_{};
    bool isMoving_{false};
    bool sprinting_{false};
    bool hadCollisionThisFrame_{false};
};
