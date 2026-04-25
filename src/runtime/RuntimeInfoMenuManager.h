#pragma once
// MIGRADO DESDE main.cpp: Líneas ~710-1095 (drawRaylibInfoMenu)
// Responsabilidad: Renderizar menú de información principal con controles de ruta y académico

#include "../core/graph/CampusGraph.h"
#include "../core/runtime/SceneRuntimeTypes.h"
#include "../services/ScenarioManager.h"
#include "../services/RuntimeBlockerService.h"
#include "../services/DestinationCatalog.h"
#include "../ui/TabManager.h"
#include <vector>
#include <string>

struct InfoMenuContext {
    bool& isOpen;
    int screenWidth;
    int screenHeight;
    int& selectedRouteSceneIdx;
    bool& routeActive;
    std::string& routeTargetScene;
    float& routeProgressPct;
    float& routeTravelElapsed;
    bool& routeTravelCompleted;
    float& routeLegStartDistance;
    std::string& routeLegSceneId;
    std::string& routeLegNextSceneId;
    std::vector<std::string>& routeScenePlan;
    std::vector<Vector2>& routePathPoints;
    std::string& routeNextHint;
    float& routeRefreshCooldown;
    const std::vector<std::pair<std::string, std::string>>& routeScenes;
    std::function<std::string(const std::string&)> sceneDisplayName;
    const CampusGraph& graph;
    const TraversalResult& dfsTraversal;
    const TraversalResult& bfsTraversal;
    int& rubricViewMode;
    int& graphPage;
    int& dfsPage;
    int& bfsPage;
    bool& showNavigationGraph;
    TabManagerState& state;
    const std::string& currentSceneName;
    bool showHitboxes;
    bool showTriggers;
    bool showInterestZones;
    ScenarioManager& scenarioManager;
    RuntimeBlockerService& runtimeBlockerService;
    const DestinationCatalog& destinationCatalog;
    int& selectedBlockedNodeIdx;
    int& selectedBlockedEdgeIdx;
    ResilienceService& resilienceService;
};

class RuntimeInfoMenuManager {
public:
    // MIGRADO DESDE main.cpp:710-1095
    static void draw(InfoMenuContext& ctx);
    
private:
    static bool drawRayButton(const Rectangle& r, const char* label, int fontSize,
                              Color base, Color hover, Color active, Color textColor);
};
