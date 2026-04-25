#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1116-1490 (inicialización completa del juego)
// Responsabilidad: Orquestar inicialización de todos los sistemas y recursos

#include <raylib.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "core/graph/CampusGraph.h"
#include "services/NavigationService.h"
#include "services/DataManager.h"
#include "services/ScenarioManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/ResilienceService.h"
#include "services/DestinationCatalog.h"
#include "services/RuntimeBlockerService.h"
#include "services/TransitionService.h"
#include "core/runtime/SceneRuntimeTypes.h"
#include "ui/TabManager.h"
#include "runtime/RuntimeInputManager.h"
#include "runtime/RuntimeNavigationManager.h"
#include "runtime/RuntimeAnimationManager.h"

struct GameContext {
    // Servicios principales
    CampusGraph graph;
    NavigationService navService;
    ScenarioManager scenarioManager;
    ComplexityAnalyzer complexityAnalyzer;
    ResilienceService resilienceService;
    DestinationCatalog destinationCatalog;
    RuntimeBlockerService runtimeBlockerService;
    TransitionService transitions;
    
    // Managers runtime
    RuntimeInputManager inputManager;
    RuntimeNavigationManager runtimeNavigation;
    
    // Datos de escenas
    std::unordered_map<std::string, SceneConfig> sceneMap;
    std::unordered_map<std::string, SceneData> sceneDataMap;
    std::vector<SceneLink> sceneLinks;
    std::vector<std::pair<std::string, std::string>> routeScenes;
    
    // Estado UI
    TabManagerState tabState;
    
    // Configuración
    std::string currentSceneName;
    int screenWidth{1280};
    int screenHeight{720};
    float pixelsToMeters{0.10f};
    
    GameContext();
};

class RuntimeInitManager {
public:
    // MIGRADO DESDE main.cpp:1119-1174 (carga de grafo y datos)
    static bool loadCampusData(GameContext& ctx, const std::string& campusJsonPath, int argc, char* argv[]);
    
    // MIGRADO DESDE main.cpp:1183-1210 (carga de hitboxes y TMJ)
    static void loadSceneData(GameContext& ctx, const std::vector<SceneConfig>& allScenes, 
                              const std::string& interestZonesPath, int argc, char* argv[]);
    
    // MIGRADO DESDE main.cpp:1225-1366 (transiciones y portals)
    static void setupTransitions(GameContext& ctx, const std::vector<SceneConfig>& allScenes, int argc, char* argv[]);
    
    // MIGRADO DESDE main.cpp:1368-1490 (inicialización runtime)
    static void initRuntimeState(GameContext& ctx, const std::string& initialSceneName, 
                                 SpriteAnim& playerAnim, Vector2& playerPos, Camera2D& camera,
                                 int argc, char* argv[]);
    
    // Limpieza de recursos
    static void unload(GameContext& ctx, Texture2D& mapTexture, SpriteAnim& playerAnim);
};
