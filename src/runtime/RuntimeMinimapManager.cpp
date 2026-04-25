#include "RuntimeMinimapManager.h"
#include "../services/WalkablePathService.h"
#include <algorithm>

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1727-1821 (render minimap)
// ============================================================================

static Vector2 playerFrontAnchor(const Vector2& playerPos, int direction) {
    constexpr float kOffset = 14.0f;
    switch (direction) {
        case 1: return Vector2{playerPos.x - kOffset, playerPos.y - 6.0f}; // left
        case 2: return Vector2{playerPos.x + kOffset, playerPos.y - 6.0f}; // right
        case 3: return Vector2{playerPos.x, playerPos.y - kOffset};        // up
        case 0:
        default: return Vector2{playerPos.x, playerPos.y + kOffset};       // down
    }
}

void RuntimeMinimapManager::render(
    const MapRenderData& mapData,
    const Vector2& playerPos,
    int playerDirection,
    int screenWidth,
    int screenHeight,
    const std::vector<Rectangle>& runtimeBlockers,
    bool routeActive,
    const std::vector<Vector2>& routePathPoints,
    const std::string& routePathScene,
    const std::string& currentSceneName) {
    
    if (!mapData.hasTexture) return;
    
    constexpr int kMapW = 200;
    constexpr int kMapH = 150;
    constexpr int kMapPad = 12;
    constexpr float kWorldRadius = 300.0f;

    const int mapX = screenWidth - kMapW - kMapPad;
    const int mapY = screenHeight - kMapH - kMapPad;

    const float texW = static_cast<float>(mapData.texture.width);
    const float texH = static_cast<float>(mapData.texture.height);
    const float srcW = std::min(2.0f * kWorldRadius, texW);
    const float srcH = std::min(2.0f * kWorldRadius, texH);
    const float srcX = std::clamp(playerPos.x - srcW * 0.5f, 0.0f, std::max(0.0f, texW - srcW));
    const float srcY = std::clamp(playerPos.y - srcH * 0.5f, 0.0f, std::max(0.0f, texH - srcH));
    const Rectangle srcRect{srcX, srcY, srcW, srcH};

    auto worldToMini = [&](const Vector2& p) {
        return Vector2{
            static_cast<float>(mapX) + ((p.x - srcRect.x) / srcRect.width) * kMapW,
            static_cast<float>(mapY) + ((p.y - srcRect.y) / srcRect.height) * kMapH
        };
    };
    auto clampMiniPoint = [&](Vector2 p) {
        p.x = std::clamp(p.x, static_cast<float>(mapX), static_cast<float>(mapX + kMapW));
        p.y = std::clamp(p.y, static_cast<float>(mapY), static_cast<float>(mapY + kMapH));
        return p;
    };

    // Draw minimap background
    DrawRectangle(mapX - 2, mapY - 2, kMapW + 4, kMapH + 4, Color{0, 0, 0, 200});
    
    // Draw scene texture cropped around player
    DrawTexturePro(mapData.texture,
                   srcRect,
                   Rectangle{static_cast<float>(mapX), static_cast<float>(mapY),
                             static_cast<float>(kMapW), static_cast<float>(kMapH)},
                   Vector2{0, 0}, 0.0f, Color{255, 255, 255, 210});

    // Draw hitboxes
    for (const auto& hitbox : mapData.hitboxes) {
        const float left = std::max(hitbox.x, srcRect.x);
        const float top = std::max(hitbox.y, srcRect.y);
        const float right = std::min(hitbox.x + hitbox.width, srcRect.x + srcRect.width);
        const float bottom = std::min(hitbox.y + hitbox.height, srcRect.y + srcRect.height);
        if (right <= left || bottom <= top) continue;

        DrawRectangleRec(Rectangle{
            static_cast<float>(mapX) + ((left - srcRect.x) / srcRect.width) * kMapW,
            static_cast<float>(mapY) + ((top - srcRect.y) / srcRect.height) * kMapH,
            ((right - left) / srcRect.width) * kMapW,
            ((bottom - top) / srcRect.height) * kMapH
        }, Color{120, 120, 120, 135});
    }

    // Draw runtime blockers
    for (const auto& blockedRect : runtimeBlockers) {
        const float left = std::max(blockedRect.x, srcRect.x);
        const float top = std::max(blockedRect.y, srcRect.y);
        const float right = std::min(blockedRect.x + blockedRect.width, srcRect.x + srcRect.width);
        const float bottom = std::min(blockedRect.y + blockedRect.height, srcRect.y + srcRect.height);
        if (right <= left || bottom <= top) continue;

        DrawRectangleRec(Rectangle{
            static_cast<float>(mapX) + ((left - srcRect.x) / srcRect.width) * kMapW,
            static_cast<float>(mapY) + ((top - srcRect.y) / srcRect.height) * kMapH,
            ((right - left) / srcRect.width) * kMapW,
            ((bottom - top) / srcRect.height) * kMapH
        }, Color{220, 70, 70, 145});
    }

    // Draw route path
    if (routeActive && routePathScene == currentSceneName && routePathPoints.size() >= 2) {
        const Vector2 routeStart = playerFrontAnchor(playerPos, playerDirection);
        for (size_t i = 1; i < routePathPoints.size(); ++i) {
            const Vector2 worldA = (i == 1) ? routeStart : routePathPoints[i - 1];
            const Vector2 a = clampMiniPoint(worldToMini(worldA));
            const Vector2 b = clampMiniPoint(worldToMini(routePathPoints[i]));
            DrawLineEx(a, b, 3.0f, Color{255, 210, 60, 240});
        }
        const Vector2 goalMarker = clampMiniPoint(worldToMini(routePathPoints.back()));
        DrawCircleV(goalMarker, 5.0f, Color{255, 180, 60, 240});
        DrawCircleLines(static_cast<int>(goalMarker.x), static_cast<int>(goalMarker.y), 5.0f, BLACK);
    }

    // Draw player dot
    const Vector2 playerMini = clampMiniPoint(worldToMini(playerPos));
    DrawCircle(static_cast<int>(playerMini.x), static_cast<int>(playerMini.y), 4.0f, Color{0, 220, 255, 255});
    DrawCircleLines(static_cast<int>(playerMini.x), static_cast<int>(playerMini.y), 4.0f, WHITE);

    // Minimap border and label
    DrawRectangleLines(mapX - 2, mapY - 2, kMapW + 4, kMapH + 4, Color{80, 160, 255, 200});
    DrawText("Map", mapX + 4, mapY + 4, 12, Color{180, 220, 255, 220});
    if (routeActive) {
        DrawText("Route active", mapX + 52, mapY + 4, 12, Color{255, 220, 120, 220});
    }
}
