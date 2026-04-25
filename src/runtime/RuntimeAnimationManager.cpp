#include "RuntimeAnimationManager.h"
#include "../services/SpriteAnimationService.h"
#include <algorithm>

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1400-1424 (loadPlayerSprites)
// ============================================================================

void RuntimeAnimationManager::loadPlayerSprites(SpriteAnim& anim, const std::string& idlePath, const std::string& walkPath) {
    if (!idlePath.empty()) {
        anim.idle = LoadTexture(idlePath.c_str());
        anim.hasIdle = anim.idle.id != 0;
    }
    if (!walkPath.empty()) {
        anim.walk = LoadTexture(walkPath.c_str());
        anim.hasWalk = anim.walk.id != 0;
    }

    if (anim.hasIdle) {
        anim.frameHeight = anim.idle.height;
        anim.frameWidth = 16;
        if (anim.idle.width % anim.frameWidth != 0) {
            anim.frameWidth = std::max(1, anim.idle.height);
        }
        anim.idleFrames = std::max(1, anim.idle.width / anim.frameWidth);
    }
    if (anim.hasWalk) {
        if (anim.walk.width % anim.frameWidth != 0) {
            anim.frameWidth = std::max(1, anim.walk.height);
        }
        anim.walkFrames = std::max(1, anim.walk.width / std::max(1, anim.frameWidth));
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1609-1620 (update animation state)
// ============================================================================

void RuntimeAnimationManager::update(SpriteAnim& anim, bool isMoving, float dt, bool sprinting) {
    if (isMoving) {
        anim.timer += dt;
        const float frameStep = sprinting ? (1.0f / 16.0f) : (1.0f / 12.0f);
        if (anim.timer >= frameStep) {
            anim.timer = 0.0f;
            anim.frame = (anim.frame + 1) % SpriteAnimationService::directionalFrameCount(anim.walkFrames);
        }
    } else {
        anim.timer = 0.0f;
        anim.frame = 0;
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1696-1721 (render player sprite)
// ============================================================================

void RuntimeAnimationManager::render(const SpriteAnim& anim, const Vector2& playerPos, float renderScale) {
    const bool useWalk = anim.hasWalk && (anim.direction != 0 || anim.frame > 0);
    const bool canDrawSprite = anim.hasIdle || anim.hasWalk;
    
    if (canDrawSprite) {
        const Texture2D tex = useWalk ? anim.walk : anim.idle;
        const int activeFrames = useWalk ? anim.walkFrames : anim.idleFrames;
        const int baseFrame = SpriteAnimationService::directionStartFrame(anim.direction, activeFrames);
        const int frameCount = SpriteAnimationService::directionalFrameCount(activeFrames);
        const int activeFrame = baseFrame + (anim.frame % frameCount);
        const float frameW = static_cast<float>(anim.frameWidth);
        const float frameH = static_cast<float>(anim.frameHeight);
        
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
// NUEVO: Unload textures
// ============================================================================

void RuntimeAnimationManager::unload(SpriteAnim& anim) {
    if (anim.hasIdle) UnloadTexture(anim.idle);
    if (anim.hasWalk) UnloadTexture(anim.walk);
    anim.hasIdle = false;
    anim.hasWalk = false;
}
