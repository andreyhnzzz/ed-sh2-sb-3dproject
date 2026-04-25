#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1491-1932 (game loop completo)
// Responsabilidad: Estructura de estado del juego y orquestación del game loop

#include <raylib.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "core/graph/CampusGraph.h"
#include "core/runtime/SceneRuntimeTypes.h"
#include "services/NavigationService.h"
#include "services/ScenarioManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/ResilienceService.h"
#include "services/DestinationCatalog.h"
#include "services/RuntimeBlockerService.h"
#include "services/TransitionService.h"
#include "ui/TabManager.h"
#include "runtime/RuntimeInputManager.h"
#include "runtime/RuntimeNavigationManager.h"

struct GameState {
    // Estado del jugador
    Vector2 playerPos{0.0f, 0.0f};
    float playerSpeed{150.0f};
    float sprintMultiplier{1.8f};
    float playerRenderScale{1.6f};
    SpriteAnim playerAnim;
    
    // Cámara
    Camera2D camera{};
    float minZoom{1.2f};
    float maxZoom{4.0f};
    
    // Escena actual
    std::string currentSceneName;
    MapRenderData mapData;
    bool showHitboxes{false};
    bool showTriggers{false};
    bool showInterestZones{true};
    
    // UI State
    bool infoMenuOpen{false};
    bool showNavigationGraph{false};
    TabManagerState tabState;
    RouteRuntimeState routeState;
    int rubricViewMode{0};
    int graphViewPage{0};
    int dfsViewPage{0};
    int bfsViewPage{0};
    int selectedBlockedNodeIdx{0};
    int selectedBlockedEdgeIdx{0};
    float traversalRefreshCooldown{0.0f};
    
    // Traversal views
    TraversalResult dfsTraversalView;
    TraversalResult bfsTraversalView;
    
    // Helpers
    std::function<std::string(const std::string&)> sceneDisplayName;
    std::function<Vector2(const std::string&)> sceneTargetPoint;
};

class RuntimeGameManager {
public:
    // MIGRADO DESDE main.cpp:1497-1540 (update loop - input, movimiento, cámara)
    static void update(GameState& state,
                       const RuntimeInputState& inputState,
                       const MapRenderData& mapData,
                       float dt);
    
    // MIGRADO DESDE main.cpp:1542-1570 (navigation refresh)
    static void updateNavigation(GameState& state,
                                 RuntimeNavigationManager& runtimeNavigation,
                                 const CampusGraph& graph,
                                 ScenarioManager& scenarioManager,
                                 ComplexityAnalyzer& complexityAnalyzer,
                                 const std::vector<SceneLink>& sceneLinks,
                                 float dt);
    
    // MIGRADO DESDE main.cpp:1572-1607 (transitions)
    static void updateTransitions(GameState& state,
                                  TransitionService& transitions,
                                  const std::unordered_map<std::string, SceneConfig>& sceneMap,
                                  const std::unordered_map<std::string, SceneData>& sceneDataMap,
                                  RuntimeBlockerService& runtimeBlockerService,
                                  float dt);
    
    // MIGRADO DESDE main.cpp:1622-1930 (render loop completo)
    static void render(const GameState& state,
                       const CampusGraph& graph,
                       const std::vector<SceneLink>& sceneLinks,
                       const DestinationCatalog& destinationCatalog,
                       const ResilienceService& resilienceService,
                       const TabManagerState& tabState,
                       TransitionService& transitions,
                       int screenWidth,
                       int screenHeight);
};
