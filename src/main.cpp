#include <raylib.h>
#include "rlImGui.h"

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
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
#include "services/IntroTourConfigLoader.h"
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
using json = nlohmann::json;
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

    const std::vector<std::string> defaultIntroSceneOrder = {
        "Paradadebus",
        "Exteriorcafeteria",
        "piso4",
        "piso1",
        "piso2",
        "piso3",
        "piso5",
        "piso4",
        "Exteriorcafeteria",
        "biblio",
        "Exteriorcafeteria",
        "Interiorcafeteria",
        "Exteriorcafeteria",
        "Paradadebus"
    };
    const std::string introTourConfigPath =
        AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, "assets/intro_tour_paths.json");
    const std::string introTourRecordingsPath =
        AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, "assets/intro_tour_recordings.json");
    const IntroTourConfig introTourConfig = loadIntroTourConfig(introTourConfigPath);
    const std::vector<std::string> introSceneOrder =
        introTourConfig.sceneOrder.size() > 1 ? introTourConfig.sceneOrder : defaultIntroSceneOrder;
    const float introSecondsPerScene = std::clamp(introTourConfig.secondsPerScene, 5.0f, 90.0f);
    const float introTransitionDuration = std::clamp(introTourConfig.transitionSeconds, 0.8f, 8.0f);
    const float introCameraZoom = std::clamp(introTourConfig.cameraZoom, 1.2f, 4.0f);
    const float introCameraFollowLerp = std::clamp(introTourConfig.cameraFollowLerp, 0.8f, 10.0f);
    const float introSpeedScale = 0.45f;
    const float introFollowScale = 0.55f;
    std::vector<std::vector<Vector2>> introRecordedPathsByIndex(introSceneOrder.size());
    if (!introTourRecordingsPath.empty()) {
        try {
            std::ifstream file(introTourRecordingsPath);
            if (file.is_open()) {
                json root;
                file >> root;
                if (root.contains("routes") && root["routes"].is_array()) {
                    std::vector<std::pair<std::string, std::vector<Vector2>>> loadedRoutes;
                    for (const auto& route : root["routes"]) {
                        if (!route.is_object()) continue;
                        const std::string scene = route.value("scene", "");
                        if (scene.empty()) continue;
                        std::vector<Vector2> points;
                        if (route.contains("points") && route["points"].is_array()) {
                            for (const auto& point : route["points"]) {
                                if (!point.is_array() || point.size() != 2) continue;
                                if (!point[0].is_number() || !point[1].is_number()) continue;
                                points.push_back(Vector2{
                                    std::clamp(point[0].get<float>(), 0.0f, 1.0f),
                                    std::clamp(point[1].get<float>(), 0.0f, 1.0f)
                                });
                            }
                        }
                        if (points.size() >= 2) {
                            loadedRoutes.push_back({scene, std::move(points)});
                        }
                    }

                    size_t routeCursor = 0;
                    for (size_t i = 0; i < introSceneOrder.size() && routeCursor < loadedRoutes.size(); ++i) {
                        const std::string wantedScene = canonicalSceneId(introSceneOrder[i]);
                        size_t matched = loadedRoutes.size();
                        for (size_t j = routeCursor; j < loadedRoutes.size(); ++j) {
                            if (canonicalSceneId(loadedRoutes[j].first) == wantedScene) {
                                matched = j;
                                break;
                            }
                        }
                        if (matched == loadedRoutes.size()) continue;
                        introRecordedPathsByIndex[i] = loadedRoutes[matched].second;
                        routeCursor = matched + 1;
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[IntroTour] Failed to parse intro_tour_recordings.json: "
                      << ex.what() << "\n";
        }
    }

    const std::string gameplayInitialSceneName = "Paradadebus";
    const std::string previewInitialSceneName =
        introSceneOrder.empty() ? gameplayInitialSceneName : introSceneOrder.front();
    sceneManager.loadScene(resolveSceneName(previewInitialSceneName), sceneMap, sceneDataMap, runtimeBlockerService);

    const std::string idlePath = AssetPathResolver::findPlayerIdleSprite(argc > 0 ? argv[0] : nullptr);
    const std::string walkPath = AssetPathResolver::findPlayerWalkSprite(argc > 0 ? argv[0] : nullptr);
    gameController.loadPlayerSprites(idlePath, walkPath);
    gameController.setCameraOffset(Vector2{screenWidth * 0.5f, screenHeight * 0.5f});

    const Vector2 gameplaySpawnPos = [&]() -> Vector2 {
        const auto sceneIt = allSceneSpawns.find(gameplayInitialSceneName);
        if (sceneIt != allSceneSpawns.end()) {
            const auto spawnIt = sceneIt->second.find("bus_arrive");
            if (spawnIt != sceneIt->second.end()) return spawnIt->second;
        }
        return WalkablePathService::findSpawnPoint(sceneManager.getMapData());
    }();

    const std::string generatedGraphPath =
        (fs::path(path).parent_path() / "campus.generated.json").string();
    TabManagerState tabState = createTabManagerState(graph, generatedGraphPath);
    RouteRuntimeState routeState;
    routeState.routeMobilityReduced = scenarioManager.isMobilityReduced();
    routeState.routeAnchorPos = gameplaySpawnPos;

    UIManager::State uiState;

    std::vector<Vector2> dfsOverlayPathPoints;
    std::vector<Vector2> alternateOverlayPathPoints;

    auto buildSceneTourPath = [&](size_t sceneOrderIndex,
                                  const std::string& sceneName,
                                  const std::string& nextScene,
                                  const MapRenderData& mapData) {
        std::vector<Vector2> pathPoints;
        const std::string resolvedSceneName = resolveSceneName(sceneName);
        const bool hasTexture = mapData.hasTexture;
        const float w = hasTexture ? static_cast<float>(mapData.texture.width) : 1000.0f;
        const float h = hasTexture ? static_cast<float>(mapData.texture.height) : 700.0f;

        auto toWorldPath = [&](const std::vector<Vector2>& normalized) {
            std::vector<Vector2> world;
            world.reserve(normalized.size());
            for (const Vector2 p : normalized) {
                world.push_back(Vector2{
                    std::clamp(p.x, 0.0f, 1.0f) * w,
                    std::clamp(p.y, 0.0f, 1.0f) * h
                });
            }
            return world;
        };

        if (sceneOrderIndex < introRecordedPathsByIndex.size() &&
            introRecordedPathsByIndex[sceneOrderIndex].size() >= 2) {
            pathPoints = toWorldPath(introRecordedPathsByIndex[sceneOrderIndex]);
        }

        const std::string transitionKey = makeIntroTransitionKey(sceneName, nextScene);
        if (pathPoints.size() < 2) {
            if (const auto transitionIt = introTourConfig.transitionPaths.find(transitionKey);
                transitionIt != introTourConfig.transitionPaths.end()) {
                pathPoints = toWorldPath(transitionIt->second);
            }
        }

        if (pathPoints.size() < 2) {
            std::string sceneKey = sceneName;
            std::transform(sceneKey.begin(), sceneKey.end(), sceneKey.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (const auto fallbackIt = introTourConfig.sceneFallbackPaths.find(sceneKey);
                fallbackIt != introTourConfig.sceneFallbackPaths.end()) {
                pathPoints = toWorldPath(fallbackIt->second);
            }
        }

        if (pathPoints.size() < 2) {
            const auto spawnIt = allSceneSpawns.find(resolvedSceneName);
            if (spawnIt != allSceneSpawns.end()) {
                std::vector<std::string> sortedKeys;
                sortedKeys.reserve(spawnIt->second.size());
                for (const auto& [spawnId, _] : spawnIt->second) {
                    sortedKeys.push_back(spawnId);
                }
                std::sort(sortedKeys.begin(), sortedKeys.end());
                for (const std::string& spawnId : sortedKeys) {
                    const auto posIt = spawnIt->second.find(spawnId);
                    if (posIt != spawnIt->second.end()) pathPoints.push_back(posIt->second);
                }
            }
        }

        if (pathPoints.empty()) {
            pathPoints = {
                Vector2{w * 0.20f, h * 0.75f},
                Vector2{w * 0.42f, h * 0.58f},
                Vector2{w * 0.64f, h * 0.44f},
                Vector2{w * 0.82f, h * 0.30f}
            };
        }

        std::vector<Vector2> filtered;
        filtered.reserve(pathPoints.size());
        for (const Vector2& p : pathPoints) {
            if (filtered.empty()) {
                filtered.push_back(p);
                continue;
            }
            const float dx = p.x - filtered.back().x;
            const float dy = p.y - filtered.back().y;
            if (std::sqrt(dx * dx + dy * dy) >= 4.0f) {
                filtered.push_back(p);
            }
        }
        if (!filtered.empty()) {
            pathPoints = std::move(filtered);
        }

        return pathPoints;
    };

    auto scenePathLength = [](const std::vector<Vector2>& points) {
        if (points.size() < 2) return 0.0f;
        float length = 0.0f;
        for (size_t i = 1; i < points.size(); ++i) {
            const float dx = points[i].x - points[i - 1].x;
            const float dy = points[i].y - points[i - 1].y;
            length += std::sqrt(dx * dx + dy * dy);
        }
        return length;
    };

    auto clampIntroCamera = [&](Camera2D& camera, const MapRenderData& mapData) {
        if (!mapData.hasTexture) return;

        const float halfViewWidth = (static_cast<float>(screenWidth) * 0.5f) / camera.zoom;
        const float halfViewHeight = (static_cast<float>(screenHeight) * 0.5f) / camera.zoom;
        const float minX = halfViewWidth;
        const float maxX = static_cast<float>(mapData.texture.width) - halfViewWidth;
        const float minY = halfViewHeight;
        const float maxY = static_cast<float>(mapData.texture.height) - halfViewHeight;

        if (minX > maxX) camera.target.x = static_cast<float>(mapData.texture.width) * 0.5f;
        else camera.target.x = std::clamp(camera.target.x, minX, maxX);

        if (minY > maxY) camera.target.y = static_cast<float>(mapData.texture.height) * 0.5f;
        else camera.target.y = std::clamp(camera.target.y, minY, maxY);
    };

    enum class AppMode {
        StartMenu,
        Gameplay
    };

    struct IntroTourState {
        size_t sceneIndex{0};
        size_t waypointIndex{0};
        std::vector<Vector2> path;
        Vector2 virtualPlayerPos{0.0f, 0.0f};
        float travelSpeed{110.0f};
        float transitionTimer{0.0f};
        bool completedSceneRoute{false};
        bool transitioning{false};
        bool swappedAtMidpoint{false};
    };

    constexpr bool kIntroMenuEnabled = true;
    AppMode appMode = kIntroMenuEnabled ? AppMode::StartMenu : AppMode::Gameplay;
    bool exitRequested = false;
    int menuSelection = 0;
    IntroTourState intro;
    const std::string previewNextSceneName =
        introSceneOrder.size() > 1 ? introSceneOrder[1] : previewInitialSceneName;
    intro.path = buildSceneTourPath(0, previewInitialSceneName, previewNextSceneName, sceneManager.getMapData());
    intro.waypointIndex = intro.path.size() > 1 ? 1 : 0;
    intro.virtualPlayerPos = intro.path.front();
    intro.travelSpeed = std::clamp((scenePathLength(intro.path) / introSecondsPerScene) * introSpeedScale,
                                   24.0f, 92.0f);

    Camera2D introCamera{};
    introCamera.offset = Vector2{screenWidth * 0.5f, screenHeight * 0.5f};
    introCamera.target = intro.virtualPlayerPos;
    introCamera.rotation = 0.0f;
    introCamera.zoom = introCameraZoom;

    auto startGameplay = [&]() {
        sceneManager.loadScene(gameplayInitialSceneName, sceneMap, sceneDataMap, runtimeBlockerService);
        gameController.setPlayerPosition(gameplaySpawnPos);
        gameController.resetZoom();
        gameController.clampCameraToMap(sceneManager.getMapData(), screenWidth, screenHeight);

        uiState = UIManager::State{};
        uiManager.refreshTraversalViews(uiState, canonicalSceneId(gameplayInitialSceneName), navService,
                                        scenarioManager.isMobilityReduced());

        routeState = RouteRuntimeState{};
        routeState.routeMobilityReduced = scenarioManager.isMobilityReduced();
        routeState.routeAnchorPos = gameplaySpawnPos;

        dfsOverlayPathPoints.clear();
        alternateOverlayPathPoints.clear();
    };

    if (!kIntroMenuEnabled) {
        startGameplay();
    }

    while (!WindowShouldClose() && !exitRequested) {
        const float dt = GetFrameTime();

        if (appMode == AppMode::StartMenu) {
            const MapRenderData& previewMap = sceneManager.getMapData();

            if (!intro.transitioning) {
                intro.completedSceneRoute = false;
                if (intro.path.size() > 1 && intro.waypointIndex < intro.path.size()) {
                    float remainingStep = intro.travelSpeed * dt;
                    size_t safeGuard = 0;
                    while (remainingStep > 0.0f && safeGuard < intro.path.size() * 2) {
                        const Vector2 target = intro.path[intro.waypointIndex];
                        Vector2 delta{target.x - intro.virtualPlayerPos.x, target.y - intro.virtualPlayerPos.y};
                        const float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);

                        if (dist <= 0.001f) {
                            if (intro.waypointIndex + 1 >= intro.path.size()) {
                                intro.completedSceneRoute = true;
                                remainingStep = 0.0f;
                                break;
                            }
                            ++intro.waypointIndex;
                            ++safeGuard;
                            continue;
                        }

                        if (remainingStep >= dist) {
                            intro.virtualPlayerPos = target;
                            remainingStep -= dist;
                            if (intro.waypointIndex + 1 >= intro.path.size()) {
                                intro.completedSceneRoute = true;
                                remainingStep = 0.0f;
                                break;
                            }
                            ++intro.waypointIndex;
                        } else {
                            intro.virtualPlayerPos.x += (delta.x / dist) * remainingStep;
                            intro.virtualPlayerPos.y += (delta.y / dist) * remainingStep;
                            remainingStep = 0.0f;
                        }
                    }
                }

                if (intro.completedSceneRoute || intro.path.size() <= 1) {
                    intro.transitioning = true;
                    intro.transitionTimer = 0.0f;
                    intro.swappedAtMidpoint = false;
                }
            } else {
                intro.transitionTimer += dt;
                const float transitionDuration = introTransitionDuration;
                const float transitionHalf = transitionDuration * 0.5f;

                if (!intro.swappedAtMidpoint && intro.transitionTimer >= transitionHalf) {
                    intro.sceneIndex = (intro.sceneIndex + 1) % introSceneOrder.size();
                    sceneManager.loadScene(resolveSceneName(introSceneOrder[intro.sceneIndex]), sceneMap, sceneDataMap,
                                           runtimeBlockerService);
                    const std::string currentScene = introSceneOrder[intro.sceneIndex];
                    const std::string nextScene =
                        introSceneOrder[(intro.sceneIndex + 1) % introSceneOrder.size()];
                    intro.path = buildSceneTourPath(intro.sceneIndex, currentScene, nextScene, sceneManager.getMapData());
                    intro.waypointIndex = intro.path.size() > 1 ? 1 : 0;
                    intro.virtualPlayerPos = intro.path.front();
                    intro.travelSpeed = std::clamp((scenePathLength(intro.path) / introSecondsPerScene) * introSpeedScale,
                                                   24.0f, 92.0f);
                    intro.completedSceneRoute = false;
                    intro.swappedAtMidpoint = true;
                }

                if (intro.transitionTimer >= transitionDuration) {
                    intro.transitioning = false;
                    intro.transitionTimer = 0.0f;
                    intro.swappedAtMidpoint = false;
                }
            }

            introCamera.zoom = introCameraZoom;
            introCamera.rotation = 0.0f;
            const float followFactor = std::clamp(dt * introCameraFollowLerp * introFollowScale, 0.0f, 1.0f);
            introCamera.target.x += (intro.virtualPlayerPos.x - introCamera.target.x) * followFactor;
            introCamera.target.y += (intro.virtualPlayerPos.y - introCamera.target.y) * followFactor;
            clampIntroCamera(introCamera, previewMap);

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                menuSelection = (menuSelection + 1) % 2;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                menuSelection = (menuSelection + 1) % 2;
            }

            const int panelW = 520;
            const int panelH = 300;
            const Rectangle panelRect{
                static_cast<float>((screenWidth - panelW) / 2),
                static_cast<float>((screenHeight - panelH) / 2),
                static_cast<float>(panelW),
                static_cast<float>(panelH)
            };

            const Rectangle startRect{panelRect.x + 68.0f, panelRect.y + 136.0f, panelRect.width - 136.0f, 52.0f};
            const Rectangle exitRect{panelRect.x + 68.0f, panelRect.y + 204.0f, panelRect.width - 136.0f, 52.0f};
            const Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, startRect)) menuSelection = 0;
            if (CheckCollisionPointRec(mouse, exitRect)) menuSelection = 1;

            const bool activate = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            if (activate) {
                if (menuSelection == 0) {
                    startGameplay();
                    appMode = AppMode::Gameplay;
                } else {
                    exitRequested = true;
                }
            }

            BeginDrawing();
            ClearBackground({12, 14, 20, 255});
            BeginMode2D(introCamera);
            if (previewMap.hasTexture) {
                MapRenderService::drawMapWithHitboxes(previewMap, false);
            }
            EndMode2D();

            if (intro.transitioning) {
                const float phase = intro.transitionTimer / introTransitionDuration;
                const float blend = phase < 0.5f ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
                const int alpha = static_cast<int>(std::clamp(blend * 235.0f, 0.0f, 235.0f));
                DrawRectangle(0, 0, screenWidth, screenHeight, Color{8, 10, 16, static_cast<unsigned char>(alpha)});

                const std::string nextScene = introSceneOrder[(intro.sceneIndex + 1) % introSceneOrder.size()];
                const std::string transitionText = "Transicion a " + sceneDisplayName(nextScene);
                const int txtW = MeasureText(transitionText.c_str(), 28);
                DrawText(transitionText.c_str(), (screenWidth - txtW) / 2, screenHeight - 92, 28,
                         Color{240, 240, 245, 220});
            }

            DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 120});
            DrawRectangle(static_cast<int>(panelRect.x), static_cast<int>(panelRect.y),
                          static_cast<int>(panelRect.width), static_cast<int>(panelRect.height), Color{28, 36, 78, 238});
            DrawRectangleLinesEx(panelRect, 4.0f, Color{246, 229, 126, 255});
            DrawRectangle(static_cast<int>(panelRect.x + 10.0f), static_cast<int>(panelRect.y + 10.0f),
                          static_cast<int>(panelRect.width - 20.0f), 52, Color{46, 60, 122, 250});
            DrawRectangleLinesEx(Rectangle{panelRect.x + 10.0f, panelRect.y + 10.0f, panelRect.width - 20.0f, 52.0f},
                                 3.0f, Color{255, 255, 255, 230});

            const std::string title = "EcoCampusNav";
            const int titleSize = 44;
            const int titleW = MeasureText(title.c_str(), titleSize);
            const Vector2 titleMeasure{static_cast<float>(titleW), static_cast<float>(titleSize)};
            const Vector2 titlePos{panelRect.x + (panelRect.width - titleMeasure.x) * 0.5f, panelRect.y + 22.0f};
            DrawText(title.c_str(), static_cast<int>(titlePos.x + 2.0f), static_cast<int>(titlePos.y + 2.0f), titleSize, Color{0, 0, 0, 200});
            DrawText(title.c_str(), static_cast<int>(titlePos.x), static_cast<int>(titlePos.y), titleSize, Color{255, 248, 186, 255});

            const std::string sceneLabel = "Recorrido: " + sceneDisplayName(introSceneOrder[intro.sceneIndex]);
            DrawText(sceneLabel.c_str(), static_cast<int>(panelRect.x + 30.0f), static_cast<int>(panelRect.y + 86.0f),
                     24, Color{194, 228, 255, 245});

            auto drawMenuButton = [&](const Rectangle& rect, const char* label, bool selected) {
                const Color bg = selected ? Color{230, 86, 74, 255} : Color{56, 72, 138, 245};
                const Color border = selected ? Color{255, 233, 130, 255} : Color{196, 214, 255, 240};
                DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y),
                              static_cast<int>(rect.width), static_cast<int>(rect.height), bg);
                DrawRectangleLinesEx(rect, 3.0f, border);

                const std::string text = label;
                const int btnTextSize = 38;
                const int textW = MeasureText(text.c_str(), btnTextSize);
                const int textX = static_cast<int>(rect.x + (rect.width - static_cast<float>(textW)) * 0.5f);
                const int textY = static_cast<int>(rect.y + 6.0f);
                DrawText(text.c_str(), textX + 2, textY + 2, btnTextSize, Color{0, 0, 0, 180});
                DrawText(text.c_str(), textX, textY, btnTextSize, Color{255, 250, 238, 255});
                if (selected) {
                    DrawText(">", static_cast<int>(rect.x - 24.0f), static_cast<int>(rect.y + 12.0f), 38, Color{255, 237, 132, 255});
                    DrawText("<", static_cast<int>(rect.x + rect.width + 10.0f), static_cast<int>(rect.y + 12.0f), 38, Color{255, 237, 132, 255});
                }
            };

            drawMenuButton(startRect, "Iniciar Juego", menuSelection == 0);
            drawMenuButton(exitRect, "Salir", menuSelection == 1);

            DrawText("W/S o Flechas para elegir", static_cast<int>(panelRect.x + 28.0f),
                     static_cast<int>(panelRect.y + panelRect.height - 34.0f), 18, Color{220, 230, 250, 245});
            DrawText("Enter para confirmar", static_cast<int>(panelRect.x + panelRect.width - 212.0f),
                     static_cast<int>(panelRect.y + panelRect.height - 34.0f), 18, Color{220, 230, 250, 245});
            EndDrawing();

            continue;
        }

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
