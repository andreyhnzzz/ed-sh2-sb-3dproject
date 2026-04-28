#include "runtime/GameplayLoopController.h"

#include "runtime/InputState.h"
#include "runtime/RenderContext.h"
#include "services/AssetPathResolver.h"
#include "services/MapRenderService.h"

#include <algorithm>

GameplayLoopController::GameplayLoopController(
    int screenWidth,
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
    EasterEggManager& easterEggManager,
    const SceneBootstrap& sceneBootstrap,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    TabManagerState& tabState,
    UIManager::State& uiState,
    RouteRuntimeState& routeState,
    const char* executablePath,
    std::function<std::string(const std::string&)> canonicalSceneId,
    std::function<std::string(const std::string&)> sceneDisplayName,
    std::function<Vector2(const std::string&)> sceneTargetPoint)
    : screenWidth_(screenWidth),
      screenHeight_(screenHeight),
      graph_(graph),
      navigationService_(navigationService),
      scenarioManager_(scenarioManager),
      complexityAnalyzer_(complexityAnalyzer),
      resilienceService_(resilienceService),
      destinationCatalog_(destinationCatalog),
      sceneLinks_(sceneLinks),
      inputManager_(inputManager),
      gameController_(gameController),
      sceneManager_(sceneManager),
      uiManager_(uiManager),
      runtimeNavigation_(runtimeNavigation),
      transitions_(transitions),
      runtimeBlockerService_(runtimeBlockerService),
      musicService_(musicService),
      soundEffectService_(soundEffectService),
      easterEggManager_(easterEggManager),
      sceneBootstrap_(sceneBootstrap),
      routeScenes_(routeScenes),
      tabState_(tabState),
      uiState_(uiState),
      routeState_(routeState),
      executablePath_(executablePath),
      canonicalSceneId_(std::move(canonicalSceneId)),
      sceneDisplayName_(std::move(sceneDisplayName)),
      sceneTargetPoint_(std::move(sceneTargetPoint)) {}

void GameplayLoopController::begin(const std::string& initialSceneName, const Vector2& spawnPos) {
    sceneManager_.loadScene(initialSceneName,
                            sceneBootstrap_.sceneMap,
                            sceneBootstrap_.sceneDataMap,
                            runtimeBlockerService_);
    gameController_.setPlayerPosition(spawnPos);
    gameController_.resetZoom();
    gameController_.clampCameraToMap(sceneManager_.getMapData(), screenWidth_, screenHeight_);

    uiState_ = UIManager::State{};
    uiManager_.refreshTraversalViews(uiState_,
                                     canonicalSceneId_(initialSceneName),
                                     navigationService_,
                                     scenarioManager_.isMobilityReduced());

    routeState_ = RouteRuntimeState{};
    routeState_.routeMobilityReduced = scenarioManager_.isMobilityReduced();
    routeState_.routeAnchorPos = spawnPos;

    clearNavigationOverlays();
    previousRouteCompleted_ = false;
    wallBumpCooldown_ = 0.0f;
    easterEggManager_.reset();

    musicService_.playGameplayMusic();
}

void GameplayLoopController::runFrame(float dt) {
    easterEggManager_.update(dt);
    wallBumpCooldown_ = std::max(0.0f, wallBumpCooldown_ - dt);

    const InputState input = inputManager_.poll(uiState_.infoMenuOpen);
    uiManager_.handleInput(input, uiState_);
    if (input.toggleInfoMenu) {
        soundEffectService_.play(SoundEffectType::SelectButton);
    }

    if (!uiState_.infoMenuOpen && input.zoomWheelDelta != 0.0f) {
        gameController_.applyZoom(input.zoomWheelDelta);
    }

    const bool shouldBlockAccessibilityStairs = scenarioManager_.isMobilityReduced();
    if (runtimeBlockerService_.accessibilityStairBlocksEnabled() != shouldBlockAccessibilityStairs) {
        runtimeBlockerService_.setAccessibilityStairBlocks(
            shouldBlockAccessibilityStairs, resilienceService_, sceneLinks_);
        routeState_.routeRefreshCooldown = 0.0f;
    }

    sceneManager_.refreshHitboxes(sceneBootstrap_.sceneDataMap, runtimeBlockerService_);
    gameController_.update(dt, input, sceneManager_.getMapData());
    if (gameController_.hadCollisionThisFrame() && wallBumpCooldown_ <= 0.0f) {
        soundEffectService_.play(SoundEffectType::WallBump);
        wallBumpCooldown_ = 0.25f;
    }
    gameController_.clampCameraToMap(sceneManager_.getMapData(), screenWidth_, screenHeight_);

    if (easterEggManager_.consumeActivationEvent()) {
        soundEffectService_.play(SoundEffectType::ItsMe);
        musicService_.playMusic("easter_egg");
    }

    EasterEggManager::TeleportRequest teleportRequest;
    if (easterEggManager_.consumePendingTeleport(teleportRequest)) {
        sceneManager_.loadScene(teleportRequest.sceneName,
                                sceneBootstrap_.sceneMap,
                                sceneBootstrap_.sceneDataMap,
                                runtimeBlockerService_);
        gameController_.setPlayerPosition(teleportRequest.spawnPos);
        gameController_.resetZoom();
        gameController_.clampCameraToMap(sceneManager_.getMapData(), screenWidth_, screenHeight_);
        routeState_ = RouteRuntimeState{};
        routeState_.routeMobilityReduced = scenarioManager_.isMobilityReduced();
        routeState_.routeAnchorPos = teleportRequest.spawnPos;
        clearNavigationOverlays();
        uiManager_.refreshTraversalViews(uiState_,
                                         canonicalSceneId_(teleportRequest.sceneName),
                                         navigationService_,
                                         scenarioManager_.isMobilityReduced());
    }

    previousRouteCompleted_ = routeState_.routeTravelCompleted;
    runtimeNavigation_.refreshRoute(routeState_,
                                    tabState_,
                                    graph_,
                                    scenarioManager_,
                                    complexityAnalyzer_,
                                    sceneLinks_,
                                    sceneManager_.getMapData(),
                                    sceneManager_.getCurrentSceneName(),
                                    gameController_.getPlayerPos(),
                                    dt,
                                    sceneDisplayName_,
                                    sceneTargetPoint_);
    if (!previousRouteCompleted_ && routeState_.routeTravelCompleted) {
        soundEffectService_.play(SoundEffectType::DestinationReached);
    }

    updateNavigationOverlays();

    uiState_.traversalRefreshCooldown -= dt;
    if (uiState_.infoMenuOpen && uiState_.traversalRefreshCooldown <= 0.0f) {
        uiManager_.refreshTraversalViews(uiState_,
                                         canonicalSceneId_(sceneManager_.getCurrentSceneName()),
                                         navigationService_,
                                         scenarioManager_.isMobilityReduced());
        uiState_.traversalRefreshCooldown = 0.2f;
    }

    sceneManager_.updateTransitions(dt,
                                    transitions_,
                                    WalkablePathService::playerColliderAt(gameController_.getPlayerPos()),
                                    uiState_.infoMenuOpen);

    Vector2 swappedSpawnPos{};
    if (sceneManager_.applyPendingSwap(transitions_,
                                       sceneBootstrap_.sceneMap,
                                       sceneBootstrap_.sceneDataMap,
                                       runtimeBlockerService_,
                                       swappedSpawnPos)) {
        gameController_.setPlayerPosition(swappedSpawnPos);
        gameController_.resetZoom();
        gameController_.clampCameraToMap(sceneManager_.getMapData(), screenWidth_, screenHeight_);
    }

    if (easterEggManager_.isActivated() &&
        sceneManager_.getCurrentSceneName() == "easter_egg" &&
        easterEggManager_.isPlayerInScreamerZone(gameController_.getPlayerPos())) {
        easterEggManager_.triggerScreamer(
            AssetPathResolver::resolveAssetPath(executablePath_, "assets/ee/jumpscare.mp4"));
    }

    BeginDrawing();
    ClearBackground({18, 20, 28, 255});
    BeginMode2D(gameController_.getCamera());

    if (sceneManager_.getMapData().hasTexture) {
        MapRenderService::drawMapWithHitboxes(sceneManager_.getMapData(), uiState_.showHitboxes);
    } else {
        DrawRectangle(0, 0, screenWidth_, screenHeight_, {22, 26, 36, 255});
    }

    if (uiState_.showInterestZones && sceneManager_.getMapData().hasTexture) {
        MapRenderService::drawInterestZones(sceneManager_.getMapData().interestZones);
    }

    RenderContext ctx{
        sceneManager_.getCurrentSceneName(),
        sceneManager_.getMapData(),
        gameController_.getPlayerPos(),
        gameController_.getPlayerAnim(),
        gameController_.getCamera(),
        uiState_.showHitboxes,
        uiState_.showTriggers,
        uiState_.showInterestZones,
        uiState_.showNavigationGraph,
        uiState_.infoMenuOpen,
        &routeState_.routePathPoints,
        &routeState_.routeScenePlan,
        &routeState_.routePathScene,
        routeState_.routeActive,
        &dfsOverlayPathPoints_,
        &alternateOverlayPathPoints_,
        screenWidth_,
        screenHeight_
    };

    uiManager_.renderWorld(ctx,
                           graph_,
                           sceneLinks_,
                           destinationCatalog_,
                           resilienceService_.getBlockedNodes(),
                           scenarioManager_.isMobilityReduced(),
                           transitions_);
    gameController_.drawPlayer();
    EndMode2D();

    uiManager_.renderScreen(ctx,
                            uiState_,
                            routeState_,
                            routeScenes_,
                            sceneDisplayName_,
                            graph_,
                            tabState_,
                            navigationService_,
                            scenarioManager_,
                            complexityAnalyzer_,
                            runtimeBlockerService_,
                            destinationCatalog_,
                            musicService_,
                            soundEffectService_,
                            resilienceService_,
                            transitions_,
                            sceneBootstrap_.sceneDataMap);
    easterEggManager_.drawScreamerOverlay(screenWidth_, screenHeight_);
    EndDrawing();
}

void GameplayLoopController::updateNavigationOverlays() {
    if (!uiState_.showNavigationGraph) {
        clearNavigationOverlays();
        return;
    }

    const std::vector<std::string> dfsOverlayNodes =
        (tabState_.hasPath && tabState_.lastAction == "PathDFS") ? tabState_.lastPath.path
                                                                 : std::vector<std::string>{};
    dfsOverlayPathPoints_ = runtimeNavigation_.buildOverlayPathForScene(sceneManager_.getCurrentSceneName(),
                                                                        dfsOverlayNodes,
                                                                        sceneLinks_,
                                                                        sceneManager_.getMapData(),
                                                                        gameController_.getPlayerPos(),
                                                                        scenarioManager_.isMobilityReduced(),
                                                                        sceneTargetPoint_);

    const std::vector<std::string> alternateOverlayNodes =
        (tabState_.hasPath &&
         (tabState_.lastAction == "AltPath" || tabState_.lastAction == "BlockEdge" ||
          tabState_.lastAction == "BlockNode"))
            ? tabState_.lastPath.path
            : std::vector<std::string>{};
    alternateOverlayPathPoints_ =
        runtimeNavigation_.buildOverlayPathForScene(sceneManager_.getCurrentSceneName(),
                                                    alternateOverlayNodes,
                                                    sceneLinks_,
                                                    sceneManager_.getMapData(),
                                                    gameController_.getPlayerPos(),
                                                    scenarioManager_.isMobilityReduced(),
                                                    sceneTargetPoint_);
}

void GameplayLoopController::clearNavigationOverlays() {
    dfsOverlayPathPoints_.clear();
    alternateOverlayPathPoints_.clear();
}
