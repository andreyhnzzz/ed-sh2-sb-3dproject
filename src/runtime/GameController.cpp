#include "GameController.h"

#include "services/SpriteAnimationService.h"
#include "services/WalkablePathService.h"

#include <algorithm>
#include <cmath>

GameController::GameController(Config config) : config_(config) {
    camera_.offset = {0.0f, 0.0f};
    camera_.target = playerPos_;
    camera_.rotation = 0.0f;
    camera_.zoom = config_.defaultZoom;
}

void GameController::update(float dt, const InputState& input, MapRenderData& mapData) {
    hadCollisionThisFrame_ = false;
    sprinting_ = input.sprinting;
    const float currentSpeed = config_.baseSpeed * (sprinting_ ? config_.sprintMultiplier : 1.0f);

    Vector2 candidate = playerPos_;
    candidate.x += input.moveX * currentSpeed * dt;
    if (mapData.hasTexture) {
        candidate.x = std::clamp(candidate.x, 8.0f, static_cast<float>(mapData.texture.width) - 8.0f);
    }
    if (!WalkablePathService::intersectsAny(WalkablePathService::playerColliderAt(candidate), mapData.hitboxes)) {
        playerPos_.x = candidate.x;
    } else if (std::fabs(input.moveX) > 0.0f) {
        hadCollisionThisFrame_ = true;
    }

    candidate = playerPos_;
    candidate.y += input.moveY * currentSpeed * dt;
    if (mapData.hasTexture) {
        candidate.y = std::clamp(candidate.y, 14.0f, static_cast<float>(mapData.texture.height));
    }
    if (!WalkablePathService::intersectsAny(WalkablePathService::playerColliderAt(candidate), mapData.hitboxes)) {
        playerPos_.y = candidate.y;
    } else if (std::fabs(input.moveY) > 0.0f) {
        hadCollisionThisFrame_ = true;
    }

    isMoving_ = (input.moveX != 0.0f || input.moveY != 0.0f);
    if (isMoving_) {
        // Keep the last real movement direction for idle facing.
        // Mapping aligned with the current spritesheet ordering:
        // W->1, S->3, A->2, D->0.
        if (std::fabs(input.moveX) >= std::fabs(input.moveY)) {
            playerAnim_.direction = (input.moveX < 0.0f) ? 2 : 0;  // left / right
        } else {
            playerAnim_.direction = (input.moveY < 0.0f) ? 1 : 3;  // up / down
        }
        playerAnim_.timer += dt;
        const float frameStep = sprinting_ ? (1.0f / 16.0f) : (1.0f / 12.0f);
        if (playerAnim_.timer >= frameStep) {
            playerAnim_.timer = 0.0f;
            playerAnim_.frame = (playerAnim_.frame + 1) %
                SpriteAnimationService::directionalFrameCount(playerAnim_.walkFrames);
        }
    } else {
        playerAnim_.timer = 0.0f;
        playerAnim_.frame = 0;
    }

    camera_.target = playerPos_;
}

void GameController::applyZoom(float wheelDelta) {
    if (wheelDelta == 0.0f) return;
    camera_.zoom = std::clamp(camera_.zoom + wheelDelta * 0.15f, config_.minZoom, config_.maxZoom);
}

void GameController::clampCameraToMap(const MapRenderData& mapData, int screenWidth, int screenHeight) {
    if (!mapData.hasTexture) return;

    const float halfViewWidth = (static_cast<float>(screenWidth) * 0.5f) / camera_.zoom;
    const float halfViewHeight = (static_cast<float>(screenHeight) * 0.5f) / camera_.zoom;
    const float minX = halfViewWidth;
    const float maxX = static_cast<float>(mapData.texture.width) - halfViewWidth;
    const float minY = halfViewHeight;
    const float maxY = static_cast<float>(mapData.texture.height) - halfViewHeight;

    if (minX > maxX) {
        camera_.target.x = static_cast<float>(mapData.texture.width) * 0.5f;
    } else {
        camera_.target.x = std::clamp(camera_.target.x, minX, maxX);
    }
    if (minY > maxY) {
        camera_.target.y = static_cast<float>(mapData.texture.height) * 0.5f;
    } else {
        camera_.target.y = std::clamp(camera_.target.y, minY, maxY);
    }
}

void GameController::drawPlayer() const {
    const bool useWalk = isMoving_ && playerAnim_.hasWalk;
    const bool canDrawSprite = playerAnim_.hasIdle || playerAnim_.hasWalk;
    if (!canDrawSprite) {
        DrawCircleV(playerPos_, 8.0f, RED);
        return;
    }

    const Texture2D tex = useWalk ? playerAnim_.walk : playerAnim_.idle;
    const int activeFrames = useWalk ? playerAnim_.walkFrames : playerAnim_.idleFrames;
    const int baseFrame = SpriteAnimationService::directionStartFrame(playerAnim_.direction, activeFrames);
    const int frameCount = SpriteAnimationService::directionalFrameCount(activeFrames);
    const int activeFrame = baseFrame + (playerAnim_.frame % frameCount);
    const float frameW = static_cast<float>(playerAnim_.frameWidth);
    const float frameH = static_cast<float>(playerAnim_.frameHeight);
    Rectangle src{frameW * static_cast<float>(activeFrame), 0.0f, frameW, frameH};
    Rectangle dst{playerPos_.x, playerPos_.y, frameW * config_.renderScale, frameH * config_.renderScale};
    DrawTexturePro(tex, src, dst, Vector2{dst.width * 0.5f, dst.height}, 0.0f, WHITE);
}

void GameController::resetZoom() {
    camera_.zoom = config_.defaultZoom;
}

void GameController::unloadPlayerSprites() {
    if (playerAnim_.hasIdle) {
        UnloadTexture(playerAnim_.idle);
        playerAnim_.idle = {};
        playerAnim_.hasIdle = false;
    }
    if (playerAnim_.hasWalk) {
        UnloadTexture(playerAnim_.walk);
        playerAnim_.walk = {};
        playerAnim_.hasWalk = false;
    }
}

void GameController::setPlayerPosition(const Vector2& pos) {
    playerPos_ = pos;
    camera_.target = pos;
}

void GameController::setCameraOffset(const Vector2& offset) {
    camera_.offset = offset;
}

void GameController::loadPlayerSprites(const std::string& idlePath, const std::string& walkPath) {
    if (!idlePath.empty()) {
        playerAnim_.idle = LoadTexture(idlePath.c_str());
        playerAnim_.hasIdle = playerAnim_.idle.id != 0;
    }
    if (!walkPath.empty()) {
        playerAnim_.walk = LoadTexture(walkPath.c_str());
        playerAnim_.hasWalk = playerAnim_.walk.id != 0;
    }

    if (playerAnim_.hasIdle) {
        playerAnim_.frameHeight = playerAnim_.idle.height;
        playerAnim_.frameWidth = 16;
        if (playerAnim_.idle.width % playerAnim_.frameWidth != 0) {
            playerAnim_.frameWidth = std::max(1, playerAnim_.idle.height);
        }
        playerAnim_.idleFrames = std::max(1, playerAnim_.idle.width / playerAnim_.frameWidth);
    }
    if (playerAnim_.hasWalk) {
        if (playerAnim_.walk.width % playerAnim_.frameWidth != 0) {
            playerAnim_.frameWidth = std::max(1, playerAnim_.walk.height);
        }
        playerAnim_.walkFrames = std::max(1, playerAnim_.walk.width / std::max(1, playerAnim_.frameWidth));
    }
}

const Camera2D& GameController::getCamera() const {
    return camera_;
}

const SpriteAnim& GameController::getPlayerAnim() const {
    return playerAnim_;
}

Vector2 GameController::getPlayerPos() const {
    return playerPos_;
}

bool GameController::isMoving() const {
    return isMoving_;
}

bool GameController::hadCollisionThisFrame() const {
    return hadCollisionThisFrame_;
}
