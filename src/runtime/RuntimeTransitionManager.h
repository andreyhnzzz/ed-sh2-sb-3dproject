#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1572-1607 (transiciones de escena)
// Responsabilidad: Gestionar transiciones entre escenas (portales, elevadores, fade)

#include <raylib.h>
#include <string>
#include <vector>
#include "core/runtime/SceneRuntimeTypes.h"
#include "services/TransitionService.h"
#include "services/WalkablePathService.h"

struct TransitionContext {
    std::string currentSceneName;
    Vector2 playerPos{0.0f, 0.0f};
    float cameraZoom{2.2f};
};

class RuntimeTransitionManager {
public:
    // MIGRADO DESDE main.cpp:1574 (update transitions)
    static void update(TransitionService& transitions, 
                       const Vector2& playerPos, 
                       const std::string& currentSceneName, 
                       float dt,
                       bool infoMenuOpen);
    
    // MIGRADO DESDE main.cpp:1578-1607 (scene swap logic)
    static bool processSceneSwap(TransitionService& transitions,
                                 TransitionContext& context,
                                 MapRenderData& mapData,
                                 const std::unordered_map<std::string, SceneConfig>& sceneMap,
                                 const std::unordered_map<std::string, SceneData>& sceneDataMap,
                                 RuntimeBlockerService& runtimeBlockerService,
                                 int screenWidth,
                                 int screenHeight);
    
    // MIGRADO DESDE main.cpp:1823-1837 (draw prompts)
    static void drawPrompts(const TransitionService& transitions, int screenWidth, int screenHeight);
    
    // MIGRADO DESDE main.cpp:1926 (floor menu)
    static void drawFloorMenu(TransitionService& transitions);
    
    // Helpers
    static bool needsSceneSwap(const TransitionService& transitions);
    static bool isPromptVisible(const TransitionService& transitions);
};
