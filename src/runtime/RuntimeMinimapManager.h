#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1727-1821 (minimap estilo Waze)
// Responsabilidad: Renderizar minimapa en esquina inferior derecha

#include <raylib.h>
#include <vector>
#include <string>
#include "core/runtime/SceneRuntimeTypes.h"

struct MinimapConfig {
    int width{200};
    int height{150};
    int padding{12};
    float worldRadius{300.0f};
};

class RuntimeMinimapManager {
public:
    // MIGRADO DESDE main.cpp:1727-1821
    static void render(
        const MapRenderData& mapData,
        const Vector2& playerPos,
        int playerDirection,
        int screenWidth,
        int screenHeight,
        const std::vector<Rectangle>& runtimeBlockers,
        bool routeActive,
        const std::vector<Vector2>& routePathPoints,
        const std::string& routePathScene,
        const std::string& currentSceneName);
};
