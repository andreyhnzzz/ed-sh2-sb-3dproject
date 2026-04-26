#pragma once

#include "../services/ComplexityAnalyzer.h"
#include "../services/DestinationCatalog.h"
#include "../services/ScenarioManager.h"
#include "../services/WalkablePathService.h"
#include "../ui/TabManager.h"

#include "../core/runtime/SceneRuntimeTypes.h"

#include <functional>
#include <string>
#include <vector>

struct RouteRuntimeState {
    int selectedDestinationIdx{0};
    bool routeActive{false};
    std::string routeTargetNodeId;
    float routeProgressPct{0.0f};
    float routeTotalDistanceMeters{0.0f};
    float routeRemainingMeters{0.0f};
    float routeTravelElapsed{0.0f};
    bool routeTravelCompleted{false};
    float routeLegStartDistance{0.0f};
    std::string routeLegSceneId;
    std::string routeLegNextSceneId;
    std::vector<std::string> routeScenePlan;
    std::vector<Vector2> routePathPoints;
    std::string routeNextHint;
    std::string routePathScene;
    bool routeMobilityReduced{false};
    float routeRefreshCooldown{0.0f};
    Vector2 routeAnchorPos{0.0f, 0.0f};
};

class RuntimeNavigationManager {
public:
    explicit RuntimeNavigationManager(const DestinationCatalog& catalog);

    const std::vector<NavigationDestination>& destinations() const;
    const NavigationDestination* selectedDestination(const RouteRuntimeState& state) const;
    const NavigationDestination* activeDestination(const RouteRuntimeState& state) const;
    std::string selectedDestinationLabel(const RouteRuntimeState& state) const;
    void cycleSelection(RouteRuntimeState& state, int delta) const;
    void activateSelectedRoute(RouteRuntimeState& state) const;
    void clearRoute(RouteRuntimeState& state, bool keepCompletedFlag = false) const;

    void refreshRoute(RouteRuntimeState& state,
                      TabManagerState& tabState,
                      const CampusGraph& graph,
                      ScenarioManager& scenarioManager,
                      ComplexityAnalyzer& complexityAnalyzer,
                      const std::vector<SceneLink>& sceneLinks,
                      const MapRenderData& mapData,
                      const std::string& currentSceneName,
                      const Vector2& playerPos,
                      float dt,
                      const std::function<std::string(const std::string&)>& sceneDisplayName,
                      const std::function<Vector2(const std::string&)>& sceneTargetPoint);

    std::vector<Vector2> buildOverlayPathForScene(const std::string& currentSceneName,
                                                  const std::vector<std::string>& pathNodes,
                                                  const std::vector<SceneLink>& sceneLinks,
                                                  const MapRenderData& mapData,
                                                  const Vector2& playerPos,
                                                  bool mobilityReduced,
                                                  const std::function<Vector2(const std::string&)>& sceneTargetPoint) const;

    std::string activeDestinationSceneId(const RouteRuntimeState& state) const;

private:
    const DestinationCatalog& catalog_;
};
