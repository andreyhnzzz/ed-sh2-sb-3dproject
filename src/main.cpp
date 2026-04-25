#include <raylib.h>
#include "rlImGui.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/runtime/SceneRuntimeTypes.h"
#include "runtime/GameController.h"
#include "runtime/InputManager.h"
#include "runtime/RenderContext.h"
#include "runtime/RuntimeNavigationManager.h"
#include "runtime/SceneManager.h"
#include "runtime/UIManager.h"
#include "services/AssetPathResolver.h"
#include "services/ComplexityAnalyzer.h"
#include "services/DataManager.h"
#include "services/DestinationCatalog.h"
#include "services/InterestZoneLoader.h"
#include "services/MapRenderService.h"
#include "services/ResilienceService.h"
#include "services/RuntimeBlockerService.h"
#include "services/ScenarioManager.h"
#include "services/ScenePlanService.h"
#include "services/StringUtils.h"
#include "services/TmjLoader.h"
#include "services/TransitionService.h"
#include "services/WalkablePathService.h"
#include "ui/TabManager.h"

namespace fs = std::filesystem;
static constexpr double kPixelsToMeters = 0.10;

int main(int argc, char* argv[]) {
    std::string path = AssetPathResolver::findCampusJson(argc > 0 ? argv[0] : nullptr);
    if (path.empty()) {
        std::cerr << "campus.json was not found. Place the file in the working directory.\n";
        return 1;
    }

    int screenWidth = 1280;
    int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "EcoCampusNav (Raylib)");
    const int monitor = GetCurrentMonitor();
    screenWidth = GetMonitorWidth(monitor);
    screenHeight = GetMonitorHeight(monitor);
    SetWindowSize(screenWidth, screenHeight);
    ToggleFullscreen();
    SetTargetFPS(60);
    rlImGuiSetup(true);

    const std::vector<SceneConfig> allScenes = {
        {"Exteriorcafeteria", "assets/maps/Exteriorcafeteria.png", "assets/maps/Exteriorcafeteria.tmj"},
        {"Paradadebus", "assets/maps/Paradadebus.png", "assets/maps/Paradadebus.tmj"},
        {"Interiorcafeteria", "assets/maps/Interiorcafeteria.png", "assets/maps/Interiorcafeteria.tmj"},
        {"biblio", "assets/maps/biblio.png", "assets/maps/biblio.tmj"},
        {"piso1", "assets/maps/piso 1.png", "assets/maps/piso1.tmj"},
        {"piso2", "assets/maps/piso2.png", "assets/maps/piso2.tmj"},
        {"piso3", "assets/maps/piso 3.png", "assets/maps/piso3.tmj"},
        {"piso4", "assets/maps/piso 4.png", "assets/maps/piso 4.tmj"},
        {"piso5", "assets/maps/piso 5.png", "assets/maps/piso 5.tmj"},
    };

    std::unordered_map<std::string, SceneConfig> sceneMap;
    for (const auto& sc : allScenes) {
        sceneMap[sc.name] = sc;
    }

    std::unordered_map<std::string, std::string> sceneToTmjPath;
    for (const auto& sc : allScenes) {
        const std::string tmjPath =
            AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (!tmjPath.empty()) {
            sceneToTmjPath[sc.name] = tmjPath;
        }
    }

    const std::string interestZonesPath =
        AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, "assets/interest_zones.json");
    const auto interestZonesByScene = InterestZoneLoader::loadFromJson(interestZonesPath);

    DataManager dataManager;
    CampusGraph graph;
    try {
        graph = dataManager.loadCampusGraph(path, sceneToTmjPath, interestZonesPath, kPixelsToMeters);
        const fs::path generatedGraphPath = fs::path(path).parent_path() / "campus.generated.json";
        dataManager.exportResolvedGraph(graph, generatedGraphPath.string());
    } catch (const std::exception& ex) {
        std::cerr << "Error loading GIS data: " << ex.what() << "\n";
        return 1;
    }

    NavigationService navService(graph);
    ScenarioManager scenarioManager;
    ComplexityAnalyzer complexityAnalyzer(graph);
    ResilienceService resilienceService(graph);
    DestinationCatalog destinationCatalog;

    std::unordered_map<std::string, SceneData> sceneDataMap;
    std::unordered_map<std::string, std::unordered_map<std::string, Vector2>> allSceneSpawns;
    std::unordered_map<std::string, std::vector<TmjFloorTriggerDef>> allFloorTriggers;
    std::vector<SceneLink> sceneLinks;

    for (const auto& sc : allScenes) {
        SceneData sceneData;
        const std::string sceneKey = StringUtils::toLowerCopy(sc.name);
        const std::string tmjPath =
            AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (!tmjPath.empty()) {
            try {
                sceneData.hitboxes = loadHitboxesFromTmj(tmjPath);
                sceneData.isValid = true;
                allSceneSpawns[sc.name] = loadSpawnsFromTmj(tmjPath);
                allFloorTriggers[sc.name] = loadFloorTriggersFromTmj(tmjPath);
            } catch (const std::exception& ex) {
                std::cerr << "Could not read " << sc.tmjPath << ": " << ex.what() << "\n";
            }
        } else {
            std::cerr << "Could not find " << sc.tmjPath << "\n";
        }

        const auto zoneIt = interestZonesByScene.find(sceneKey);
        if (zoneIt != interestZonesByScene.end()) {
            sceneData.interestZones = zoneIt->second;
        }
        sceneDataMap[sc.name] = std::move(sceneData);
    }

    std::string generatedGraphRuntimePath = (fs::path(path).parent_path() / "campus.generated.json").string();
    if (!fs::exists(generatedGraphRuntimePath)) {
        const fs::path alternateGeneratedPath = fs::path(path).parent_path() / "campus_generated.json";
        if (fs::exists(alternateGeneratedPath)) {
            generatedGraphRuntimePath = alternateGeneratedPath.string();
        }
    }
    destinationCatalog.loadFromGeneratedJson(generatedGraphRuntimePath, sceneDataMap);
    scenarioManager.setReferenceWaypoints(destinationCatalog.preferredReferenceWaypoints(graph));

    TransitionService transitions;
    for (const auto& sc : allScenes) {
        const std::string tmjPath =
            AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (tmjPath.empty()) continue;

        const auto portalDefs = loadPortalsFromTmj(tmjPath, sc.name);
        for (const auto& portalDef : portalDefs) {
            const auto targetIt = allSceneSpawns.find(portalDef.toScene);
            if (targetIt == allSceneSpawns.end()) {
                std::cerr << "[Portals] Unknown target scene '" << portalDef.toScene
                          << "' in " << sc.tmjPath << "\n";
                continue;
            }
            const auto spawnIt = targetIt->second.find(portalDef.toSpawnId);
            if (spawnIt == targetIt->second.end()) {
                std::cerr << "[Portals] Unknown spawn '" << portalDef.toSpawnId
                          << "' in scene '" << portalDef.toScene << "'\n";
                continue;
            }

            UniPortal portal;
            portal.id = portalDef.portalId;
            portal.scene = portalDef.fromScene;
            portal.triggerRect = portalDef.triggerRect;
            portal.targetScene = portalDef.toScene;
            portal.spawnPos = spawnIt->second;
            portal.requiresE = portalDef.requiresE;
            transitions.addUniPortal(portal);

            sceneLinks.push_back({
                portal.id,
                portal.scene,
                portal.targetScene,
                portal.requiresE ? "Access with E" : "Automatic access",
                portal.triggerRect,
                portal.spawnPos,
                SceneLinkType::Portal
            });
        }
    }

    const std::vector<std::pair<std::string, std::string>> floorScenes = {
        {"piso1", "Floor 1"},
        {"piso2", "Floor 2"},
        {"piso3", "Floor 3"},
        {"piso4", "Floor 4"},
        {"piso5", "Floor 5"}
    };

    auto canonicalSceneId = [](std::string sceneName) -> std::string {
        std::transform(sceneName.begin(), sceneName.end(), sceneName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return sceneName;
    };

    auto resolveSceneName = [&](const std::string& sceneId) -> std::string {
        const std::string canonical = canonicalSceneId(sceneId);
        for (const auto& [actualName, _] : sceneMap) {
            if (canonicalSceneId(actualName) == canonical) return actualName;
        }
        return sceneId;
    };

    auto sceneDisplayName = [&](const std::string& sceneName) -> std::string {
        const std::string canonical = canonicalSceneId(sceneName);
        if (const NavigationDestination* destination = destinationCatalog.findDestination(canonical)) {
            return destination->label;
        }
        return sceneName;
    };

    auto sceneTargetPoint = [&](const std::string& sceneName) -> Vector2 {
        const auto spawnMapIt = allSceneSpawns.find(resolveSceneName(sceneName));
        if (spawnMapIt == allSceneSpawns.end() || spawnMapIt->second.empty()) {
            return Vector2{0.0f, 0.0f};
        }

        const auto& spawnMap = spawnMapIt->second;
        for (const std::string& preferred : {
                 std::string("bus_arrive"),
                 std::string("ext_from_bus"),
                 std::string("intcafe_arrive"),
                 std::string("biblio_main_arrive"),
                 std::string("elevator_arrive"),
                 std::string("piso4_main_L_arrive")
             }) {
            const auto it = spawnMap.find(preferred);
            if (it != spawnMap.end()) return it->second;
        }
        return spawnMap.begin()->second;
    };

    for (const auto& [sceneName, _] : floorScenes) {
        const auto triggerIt = allFloorTriggers.find(sceneName);
        if (triggerIt == allFloorTriggers.end()) continue;

        for (const auto& trigger : triggerIt->second) {
            std::string destSpawnId;
            SceneLinkType linkType = SceneLinkType::Portal;
            if (trigger.triggerType == "elevator") destSpawnId = "elevator_arrive";
            else if (trigger.triggerType == "stair_left") destSpawnId = "stair_left_arrive";
            else if (trigger.triggerType == "stair_right") destSpawnId = "stair_right_arrive";
            else continue;

            if (trigger.triggerType == "elevator") linkType = SceneLinkType::Elevator;
            else if (trigger.triggerType == "stair_left") linkType = SceneLinkType::StairLeft;
            else if (trigger.triggerType == "stair_right") linkType = SceneLinkType::StairRight;

            FloorElevator elevator;
            elevator.id = trigger.triggerType + "_" + sceneName;
            elevator.scene = sceneName;
            elevator.triggerRect = trigger.triggerRect;

            for (const auto& [dstScene, dstLabel] : floorScenes) {
                const auto dstSpawnMapIt = allSceneSpawns.find(dstScene);
                if (dstSpawnMapIt == allSceneSpawns.end()) continue;
                const auto spawnPosIt = dstSpawnMapIt->second.find(destSpawnId);
                if (spawnPosIt == dstSpawnMapIt->second.end()) continue;

                elevator.floors.push_back({dstScene, spawnPosIt->second, dstLabel});
                if (dstScene == sceneName) continue;

                std::string accessLabel = "Access";
                if (linkType == SceneLinkType::Elevator) accessLabel = "Elevator";
                if (linkType == SceneLinkType::StairLeft) accessLabel = "Left stair";
                if (linkType == SceneLinkType::StairRight) accessLabel = "Right stair";

                sceneLinks.push_back({
                    elevator.id + "_" + dstScene,
                    sceneName,
                    dstScene,
                    accessLabel,
                    elevator.triggerRect,
                    spawnPosIt->second,
                    linkType
                });
            }

            if (!elevator.floors.empty()) {
                transitions.addFloorElevator(elevator);
            }
        }
    }

    RuntimeBlockerService runtimeBlockerService;
    runtimeBlockerService.rebuildOptions(graph, destinationCatalog, sceneLinks);
    std::vector<std::pair<std::string, std::string>> routeScenes;
    for (const auto& destination : destinationCatalog.destinations()) {
        routeScenes.push_back({destination.nodeId, destination.label});
    }

    InputManager inputManager;
    GameController gameController;
    SceneManager sceneManager(argc > 0 ? argv[0] : nullptr);
    UIManager uiManager;
    RuntimeNavigationManager runtimeNavigation(destinationCatalog);

    const std::string initialSceneName = "Paradadebus";
    sceneManager.loadScene(initialSceneName, sceneMap, sceneDataMap, runtimeBlockerService);

    const std::string idlePath = AssetPathResolver::findPlayerIdleSprite(argc > 0 ? argv[0] : nullptr);
    const std::string walkPath = AssetPathResolver::findPlayerWalkSprite(argc > 0 ? argv[0] : nullptr);
    gameController.loadPlayerSprites(idlePath, walkPath);

    const Vector2 spawnPos = [&]() -> Vector2 {
        const auto sceneIt = allSceneSpawns.find(initialSceneName);
        if (sceneIt != allSceneSpawns.end()) {
            const auto spawnIt = sceneIt->second.find("bus_arrive");
            if (spawnIt != sceneIt->second.end()) return spawnIt->second;
        }
        return WalkablePathService::findSpawnPoint(sceneManager.getMapData());
    }();
    gameController.setPlayerPosition(spawnPos);
    gameController.setCameraOffset(Vector2{screenWidth * 0.5f, screenHeight * 0.5f});

    const std::string generatedGraphPath =
        (fs::path(path).parent_path() / "campus.generated.json").string();
    TabManagerState tabState = createTabManagerState(graph, generatedGraphPath);
    RouteRuntimeState routeState;
    routeState.routeMobilityReduced = scenarioManager.isMobilityReduced();
    routeState.routeAnchorPos = spawnPos;

    UIManager::State uiState;
    uiManager.refreshTraversalViews(uiState, canonicalSceneId(initialSceneName), navService,
                                    scenarioManager.isMobilityReduced());

    std::vector<Vector2> dfsOverlayPathPoints;
    std::vector<Vector2> alternateOverlayPathPoints;

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const InputState input = inputManager.poll(uiState.infoMenuOpen);
        uiManager.handleInput(input, uiState);

        if (!uiState.infoMenuOpen && input.zoomWheelDelta != 0.0f) {
            gameController.applyZoom(input.zoomWheelDelta);
        }

        sceneManager.refreshHitboxes(sceneDataMap, runtimeBlockerService);
        gameController.update(dt, input, sceneManager.getMapData());
        gameController.clampCameraToMap(sceneManager.getMapData(), screenWidth, screenHeight);

        runtimeNavigation.refreshRoute(routeState, tabState, graph, scenarioManager,
                                       complexityAnalyzer, sceneLinks, sceneManager.getMapData(),
                                       sceneManager.getCurrentSceneName(), gameController.getPlayerPos(), dt,
                                       sceneDisplayName, sceneTargetPoint);

        if (uiState.showNavigationGraph) {
            const std::vector<std::string> dfsOverlayNodes =
                (tabState.hasPath && tabState.lastAction == "PathDFS") ? tabState.lastPath.path
                                                                       : std::vector<std::string>{};
            dfsOverlayPathPoints = runtimeNavigation.buildOverlayPathForScene(
                sceneManager.getCurrentSceneName(), dfsOverlayNodes, sceneLinks, sceneManager.getMapData(),
                gameController.getPlayerPos(), scenarioManager.isMobilityReduced(), sceneTargetPoint);

            const std::vector<std::string> alternateOverlayNodes =
                (tabState.hasPath &&
                 (tabState.lastAction == "AltPath" || tabState.lastAction == "BlockEdge" ||
                  tabState.lastAction == "BlockNode"))
                    ? tabState.lastPath.path
                    : std::vector<std::string>{};
            alternateOverlayPathPoints = runtimeNavigation.buildOverlayPathForScene(
                sceneManager.getCurrentSceneName(), alternateOverlayNodes, sceneLinks, sceneManager.getMapData(),
                gameController.getPlayerPos(), scenarioManager.isMobilityReduced(), sceneTargetPoint);
        } else {
            dfsOverlayPathPoints.clear();
            alternateOverlayPathPoints.clear();
        }

        uiState.traversalRefreshCooldown -= dt;
        if (uiState.infoMenuOpen && uiState.traversalRefreshCooldown <= 0.0f) {
            uiManager.refreshTraversalViews(uiState, canonicalSceneId(sceneManager.getCurrentSceneName()),
                                            navService, scenarioManager.isMobilityReduced());
            uiState.traversalRefreshCooldown = 0.2f;
        }

        sceneManager.updateTransitions(dt, transitions,
                                       WalkablePathService::playerColliderAt(gameController.getPlayerPos()),
                                       uiState.infoMenuOpen);

        Vector2 swappedSpawnPos{};
        if (sceneManager.applyPendingSwap(transitions, sceneMap, sceneDataMap, runtimeBlockerService,
                                          swappedSpawnPos)) {
            gameController.setPlayerPosition(swappedSpawnPos);
            gameController.resetZoom();
            gameController.clampCameraToMap(sceneManager.getMapData(), screenWidth, screenHeight);
        }

        BeginDrawing();
        ClearBackground({18, 20, 28, 255});
        BeginMode2D(gameController.getCamera());

        if (sceneManager.getMapData().hasTexture) {
            MapRenderService::drawMapWithHitboxes(sceneManager.getMapData(), uiState.showHitboxes);
        } else {
            DrawRectangle(0, 0, screenWidth, screenHeight, {22, 26, 36, 255});
        }

        if (uiState.showInterestZones && sceneManager.getMapData().hasTexture) {
            MapRenderService::drawInterestZones(sceneManager.getMapData().interestZones);
        }

        RenderContext ctx{
            sceneManager.getCurrentSceneName(),
            sceneManager.getMapData(),
            gameController.getPlayerPos(),
            gameController.getPlayerAnim(),
            gameController.getCamera(),
            uiState.showHitboxes,
            uiState.showTriggers,
            uiState.showInterestZones,
            uiState.showNavigationGraph,
            uiState.infoMenuOpen,
            &routeState.routePathPoints,
            &routeState.routeScenePlan,
            &routeState.routePathScene,
            routeState.routeActive,
            &dfsOverlayPathPoints,
            &alternateOverlayPathPoints,
            screenWidth,
            screenHeight
        };

        uiManager.renderWorld(ctx, graph, sceneLinks, destinationCatalog,
                              resilienceService.getBlockedNodes(),
                              scenarioManager.isMobilityReduced(), transitions);
        gameController.drawPlayer();
        EndMode2D();

        uiManager.renderScreen(ctx, uiState, routeState, routeScenes, sceneDisplayName, graph, tabState,
                               navService, scenarioManager, complexityAnalyzer, runtimeBlockerService,
                               destinationCatalog, resilienceService, transitions, sceneDataMap);
        EndDrawing();
    }

    sceneManager.unload();
    gameController.unloadPlayerSprites();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
