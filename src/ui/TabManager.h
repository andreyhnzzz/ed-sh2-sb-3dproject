#pragma once

#include <raylib.h>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../core/graph/CampusGraph.h"
#include "../services/ComplexityAnalyzer.h"
#include "../services/NavigationService.h"
#include "../services/ResilienceService.h"
#include "../services/ScenarioManager.h"

struct CampusValidationReport {
    int nodeCount{0};
    int edgeCount{0};
    int edgesMissingMobilityFields{0};
    int edgesWithNonPositiveWeight{0};
    bool hasBibliotecaNode{false};
    bool hasSodaOrComedorNode{false};
    std::vector<std::string> issues;
};

struct TabManagerState {
    char startId[64]{};
    char endId[64]{};
    char nodeId[64]{};

    TraversalResult lastTraversal;
    TraversalResult lastDfsTraversal;
    TraversalResult lastBfsTraversal;
    PathResult lastPath;
    bool hasTraversal{false};
    bool hasDfsTraversal{false};
    bool hasBfsTraversal{false};
    bool hasPath{false};
    bool lastConnected{false};
    std::vector<AlgorithmStats> lastStats;
    AlgorithmComparison lastComparison;
    bool hasComparison{false};
    bool hasConnectivityResult{false};
    std::string lastAction;

    CampusValidationReport validation;
};

TabManagerState createTabManagerState(const CampusGraph& graph);

void renderMinimapRouteWindow(
    int screenWidth,
    int screenHeight,
    int& selectedRouteSceneIdx,
    bool& routeActive,
    std::string& routeTargetScene,
    std::vector<std::string>& routeScenePlan,
    std::vector<Vector2>& routePathPoints,
    std::string& routeNextHint,
    float& routeRefreshCooldown,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName);

void renderAcademicControlPanel(
    TabManagerState& state,
    NavigationService& navService,
    ScenarioManager& scenarioManager,
    ComplexityAnalyzer& complexityAnalyzer,
    ResilienceService& resilienceService,
    const CampusGraph& graph,
    const std::string& currentSceneName,
    bool& showHitboxes,
    bool& showTriggers,
    bool& showInterestZones);

void renderFullScreenInfoMenu(
    bool& isOpen,
    int screenWidth,
    int screenHeight,
    int& selectedRouteSceneIdx,
    bool& routeActive,
    std::string& routeTargetScene,
    std::vector<std::string>& routeScenePlan,
    std::vector<Vector2>& routePathPoints,
    std::string& routeNextHint,
    float& routeRefreshCooldown,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName,
    TabManagerState& state,
    NavigationService& navService,
    ScenarioManager& scenarioManager,
    ComplexityAnalyzer& complexityAnalyzer,
    ResilienceService& resilienceService,
    const CampusGraph& graph,
    const std::string& currentSceneName,
    bool& showHitboxes,
    bool& showTriggers,
    bool& showInterestZones);
