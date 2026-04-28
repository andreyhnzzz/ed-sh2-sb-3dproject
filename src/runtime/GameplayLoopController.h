#pragma once

#include "runtime/GameController.h"
#include "runtime/InputManager.h"
#include "runtime/RuntimeNavigationManager.h"
#include "runtime/SceneManager.h"
#include "runtime/UIManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/DestinationCatalog.h"
#include "services/MusicService.h"
#include "services/NavigationService.h"
#include "services/ResilienceService.h"
#include "services/RuntimeBlockerService.h"
#include "services/ScenarioManager.h"
#include "services/SoundEffectService.h"
#include "services/TransitionService.h"
#include "services/WalkablePathService.h"
#include "services/initialization/SceneBootstrap.h"
#include "ui/TabManager.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class GameplayLoopController {
public:
    GameplayLoopController(int screenWidth,
                           int screenHeight,
                           CampusGraph& graph,
                           NavigationService& navigationService,
                           ScenarioManager& scenarioManager,
                           ComplexityAnalyzer& complexityAnalyzer,
                           ResilienceService& resilienceService,
                           const DestinationCatalog& destinationCatalog,
                           const std::vector<SceneLink>& sceneLinks,
                           InputManager& inputManager,
                           GameController& gameController,
                           SceneManager& sceneManager,
                           UIManager& uiManager,
                           RuntimeNavigationManager& runtimeNavigation,
                           TransitionService& transitions,
                           RuntimeBlockerService& runtimeBlockerService,
                           MusicService& musicService,
                           SoundEffectService& soundEffectService,
                           const SceneBootstrap& sceneBootstrap,
                           const std::vector<std::pair<std::string, std::string>>& routeScenes,
                           TabManagerState& tabState,
                           UIManager::State& uiState,
                           RouteRuntimeState& routeState,
                           std::function<std::string(const std::string&)> canonicalSceneId,
                           std::function<std::string(const std::string&)> sceneDisplayName,
                           std::function<Vector2(const std::string&)> sceneTargetPoint);

    void begin(const std::string& initialSceneName, const Vector2& spawnPos);
    void runFrame(float dt);

private:
    void updateNavigationOverlays();
    void clearNavigationOverlays();

    int screenWidth_{0};
    int screenHeight_{0};
    CampusGraph& graph_;
    NavigationService& navigationService_;
    ScenarioManager& scenarioManager_;
    ComplexityAnalyzer& complexityAnalyzer_;
    ResilienceService& resilienceService_;
    const DestinationCatalog& destinationCatalog_;
    const std::vector<SceneLink>& sceneLinks_;
    InputManager& inputManager_;
    GameController& gameController_;
    SceneManager& sceneManager_;
    UIManager& uiManager_;
    RuntimeNavigationManager& runtimeNavigation_;
    TransitionService& transitions_;
    RuntimeBlockerService& runtimeBlockerService_;
    MusicService& musicService_;
    SoundEffectService& soundEffectService_;
    const SceneBootstrap& sceneBootstrap_;
    const std::vector<std::pair<std::string, std::string>>& routeScenes_;
    TabManagerState& tabState_;
    UIManager::State& uiState_;
    RouteRuntimeState& routeState_;
    std::function<std::string(const std::string&)> canonicalSceneId_;
    std::function<std::string(const std::string&)> sceneDisplayName_;
    std::function<Vector2(const std::string&)> sceneTargetPoint_;
    std::vector<Vector2> dfsOverlayPathPoints_;
    std::vector<Vector2> alternateOverlayPathPoints_;
    bool previousRouteCompleted_{false};
    float wallBumpCooldown_{0.0f};
};
