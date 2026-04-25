#pragma once

#include "core/runtime/SceneRuntimeTypes.h"

#include <raylib.h>

#include <string>
#include <vector>

struct RenderContext {
    const std::string& currentSceneName;
    const MapRenderData& mapData;
    const Vector2& playerPos;
    const SpriteAnim& playerAnim;
    const Camera2D& camera;
    bool showHitboxes;
    bool showTriggers;
    bool showInterestZones;
    bool showNavigationGraph;
    bool infoMenuOpen;
    const std::vector<Vector2>* routePathPoints;
    const std::vector<std::string>* routeScenePlan;
    const std::string* routePathScene;
    bool routeActive;
    const std::vector<Vector2>* dfsOverlayPathPoints;
    const std::vector<Vector2>* alternateOverlayPathPoints;
    int screenWidth;
    int screenHeight;
};
