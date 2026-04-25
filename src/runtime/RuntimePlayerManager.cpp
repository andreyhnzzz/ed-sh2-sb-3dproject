#include "RuntimePlayerManager.h"
#include "../services/WalkablePathService.h"
#include <algorithm>

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas 1506-1534 (updateMovement)
// ============================================================================

void RuntimePlayerManager::updateMovement(PlayerState& player,
                                           const RuntimeInputState& inputState,
                                           const MapRenderData& mapData,
                                           float dt) {
    player.direction = inputState.facingDirection;
    player.sprinting = inputState.sprinting;
    
    const float currentSpeed = player.speed * (player.sprinting ? player.sprintMultiplier : 1.0f);
    
    // Move X axis
    Vector2 candidate = player.position;
    candidate.x += inputState.moveX * currentSpeed * dt;
    if (mapData.hasTexture) {
        candidate.x = std::clamp(candidate.x, 8.0f, static_cast<float>(mapData.texture.width) - 8.0f);
    }
    if (!WalkablePathService::intersectsAny(WalkablePathService::playerColliderAt(candidate), mapData.hitboxes)) {
        player.position.x = candidate.x;
    }

    // Move Y axis
    candidate = player.position;
    candidate.y += inputState.moveY * currentSpeed * dt;
    if (mapData.hasTexture) {
        candidate.y = std::clamp(candidate.y, 14.0f, static_cast<float>(mapData.texture.height));
    }
    if (!WalkablePathService::intersectsAny(WalkablePathService::playerColliderAt(candidate), mapData.hitboxes)) {
        player.position.y = candidate.y;
    }
    
    player.isMoving = (inputState.moveX != 0.0f || inputState.moveY != 0.0f);
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas 1609-1620 (updateAnimation)
// ============================================================================

void RuntimePlayerManager::updateAnimation(SpriteAnim& playerAnim, float dt) {
    if (playerAnim.isMoving) {
        playerAnim.timer += dt;
        const float frameStep = playerAnim.sprinting ? (1.0f / 16.0f) : (1.0f / 12.0f);
        if (playerAnim.timer >= frameStep) {
            playerAnim.timer = 0.0f;
            playerAnim.frame = (playerAnim.frame + 1) % SpriteAnimationService::directionalFrameCount(playerAnim.walkFrames);
        }
    } else {
        playerAnim.timer = 0.0f;
        playerAnim.frame = 0;
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas 1696-1721 (render player)
// ============================================================================

void RuntimePlayerManager::render(const SpriteAnim& playerAnim, const Vector2& playerPos, float renderScale) {
    const bool useWalk = playerAnim.isMoving && playerAnim.hasWalk;
    const bool canDrawSprite = playerAnim.hasIdle || playerAnim.hasWalk;
    
    if (canDrawSprite) {
        const Texture2D tex = useWalk ? playerAnim.walk : playerAnim.idle;
        const int activeFrames = useWalk ? playerAnim.walkFrames : playerAnim.idleFrames;
        const int baseFrame = SpriteAnimationService::directionStartFrame(playerAnim.direction, activeFrames);
        const int frameCount = SpriteAnimationService::directionalFrameCount(activeFrames);
        const int activeFrame = baseFrame + (playerAnim.frame % frameCount);
        const float frameW = static_cast<float>(playerAnim.frameWidth);
        const float frameH = static_cast<float>(playerAnim.frameHeight);
        
        Rectangle src{
            frameW * static_cast<float>(activeFrame),
            0.0f,
            frameW,
            frameH
        };
        Rectangle dst{
            playerPos.x,
            playerPos.y,
            frameW * renderScale,
            frameH * renderScale
        };
        DrawTexturePro(tex, src, dst, Vector2{dst.width * 0.5f, dst.height}, 0.0f, WHITE);
    } else {
        DrawCircleV(playerPos, 8.0f, RED);
    }
}

// ============================================================================
// Helper: playerFrontAnchor (migrado desde main.cpp:111-120 y reutilizado en minimap)
// ============================================================================

Vector2 RuntimePlayerManager::getFrontAnchor(const Vector2& playerPos, int direction) {
    constexpr float kOffset = 14.0f;
    switch (direction) {
        case 1: return Vector2{playerPos.x - kOffset, playerPos.y - 6.0f}; // left
        case 2: return Vector2{playerPos.x + kOffset, playerPos.y - 6.0f}; // right
        case 3: return Vector2{playerPos.x, playerPos.y - kOffset};        // up
        case 0:
        default: return Vector2{playerPos.x, playerPos.y + kOffset};       // down
    }
}
