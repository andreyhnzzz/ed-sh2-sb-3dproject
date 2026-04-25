#include "RuntimeTransitionManager.h"

// ============================================================================
// MIGRADO DESDE main.cpp: Línea 1574 (update transitions)
// ============================================================================

void RuntimeTransitionManager::update(TransitionService& transitions, 
                                       const Vector2& playerPos, 
                                       const std::string& currentSceneName, 
                                       float dt,
                                       bool infoMenuOpen) {
    if (!infoMenuOpen) {
        transitions.update(WalkablePathService::playerColliderAt(playerPos), currentSceneName, dt);
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas 1578-1607 (processSceneSwap)
// ============================================================================

bool RuntimeTransitionManager::processSceneSwap(TransitionService& transitions,
                                                 TransitionContext& context,
                                                 MapRenderData& mapData,
                                                 const std::unordered_map<std::string, SceneConfig>& sceneMap,
                                                 const std::unordered_map<std::string, SceneData>& sceneDataMap,
                                                 RuntimeBlockerService& runtimeBlockerService,
                                                 int screenWidth,
                                                 int screenHeight) {
    if (!transitions.needsSceneSwap()) {
        return false;
    }
    
    const TransitionRequest req = transitions.getPendingSwap();
    
    // Unload current texture
    if (mapData.hasTexture) {
        UnloadTexture(mapData.texture);
        mapData.texture = {};
        mapData.hasTexture = false;
    }
    mapData.hitboxes.clear();
    mapData.interestZones.clear();

    // Load new scene
    const auto scIt = sceneMap.find(req.targetScene);
    if (scIt != sceneMap.end()) {
        const std::string pngPath = AssetPathResolver::resolveAssetPath(nullptr, scIt->second.pngPath);
        if (!pngPath.empty()) {
            mapData.texture = LoadTexture(pngPath.c_str());
            mapData.hasTexture = mapData.texture.id != 0;
        }
        const auto sdIt = sceneDataMap.find(req.targetScene);
        if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
            refreshSceneHitboxes(mapData, sdIt->second,
                                 runtimeBlockerService.collisionRectsForScene(req.targetScene));
        }
        context.currentSceneName = req.targetScene;
    }
    
    context.playerPos = req.spawnPos;
    context.cameraZoom = 2.2f;
    
    // Clamp camera to new scene bounds (inline de clampCameraTarget para evitar dependencia circular)
    if (mapData.hasTexture) {
        const float halfViewWidth = (static_cast<float>(screenWidth) * 0.5f) / context.cameraZoom;
        const float halfViewHeight = (static_cast<float>(screenHeight) * 0.5f) / context.cameraZoom;
        const float minX = halfViewWidth;
        const float maxX = static_cast<float>(mapData.texture.width) - halfViewWidth;
        const float minY = halfViewHeight;
        const float maxY = static_cast<float>(mapData.texture.height) - halfViewHeight;
        
        if (minX <= maxX) {
            context.playerPos.x = std::clamp(context.playerPos.x, minX, maxX);
        } else {
            context.playerPos.x = static_cast<float>(mapData.texture.width) * 0.5f;
        }
        if (minY <= maxY) {
            context.playerPos.y = std::clamp(context.playerPos.y, minY, maxY);
        } else {
            context.playerPos.y = static_cast<float>(mapData.texture.height) * 0.5f;
        }
    }
    
    transitions.notifySwapDone();
    return true;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas 1823-1837 (drawPrompts)
// ============================================================================

void RuntimeTransitionManager::drawPrompts(const TransitionService& transitions, int screenWidth, int screenHeight) {
    // Fade overlay
    transitions.drawFadeOverlay(screenWidth, screenHeight);
    
    // "Presiona E" prompt
    if (transitions.isPromptVisible()) {
        const std::string hintText = transitions.getPromptHint();
        const char* hint = hintText.c_str();
        const int hintFontSz = 22;
        const int hintW = MeasureText(hint, hintFontSz);
        const int hintX = (screenWidth - hintW) / 2;
        const int hintY = screenHeight - 60;
        DrawRectangle(hintX - 10, hintY - 8, hintW + 20, hintFontSz + 16, Color{0, 0, 0, 180});
        DrawText(hint, hintX, hintY, hintFontSz, YELLOW);
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Línea 1926 (drawFloorMenu)
// ============================================================================

void RuntimeTransitionManager::drawFloorMenu(TransitionService& transitions) {
    transitions.drawFloorMenu();
}

// ============================================================================
// Helpers
// ============================================================================

bool RuntimeTransitionManager::needsSceneSwap(const TransitionService& transitions) {
    return transitions.needsSceneSwap();
}

bool RuntimeTransitionManager::isPromptVisible(const TransitionService& transitions) {
    return transitions.isPromptVisible();
}
