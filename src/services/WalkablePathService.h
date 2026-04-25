#pragma once

#include "core/runtime/SceneRuntimeTypes.h"

#include <raylib.h>

#include <vector>

class WalkablePathService {
public:
    static Vector2 rectCenter(const Rectangle& rect);
    static float distanceBetween(const Vector2& a, const Vector2& b);
    static float polylineLength(const std::vector<Vector2>& points);

    static bool intersectsAny(const Rectangle& rect, const std::vector<Rectangle>& obstacles);
    static Rectangle playerColliderAt(const Vector2& playerPos);

    static std::vector<Vector2> buildWalkablePath(
        const MapRenderData& mapData,
        const Vector2& start,
        const Vector2& goal);

    static Vector2 findSpawnPoint(const MapRenderData& mapData);
};
