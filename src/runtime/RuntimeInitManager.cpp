#include "RuntimeInitManager.h"
#include "services/AssetPathResolver.h"
#include "services/StringUtils.h"
#include "services/InterestZoneLoader.h"
#include "services/WalkablePathService.h"
#include "services/TmjLoader.h"
#include "services/ScenePlanService.h"
#include "core/graph/CampusGraph.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

GameContext::GameContext() 
    : navService(graph)
    , complexityAnalyzer(graph)
    , resilienceService(graph)
    , runtimeNavigation(destinationCatalog) {
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1119-1174 (loadCampusData)
// ============================================================================

bool RuntimeInitManager::loadCampusData(GameContext& ctx, const std::string& campusJsonPath, int argc, char* argv[]) {
    DataManager dataManager;
    try {
        // Build sceneToTmjPath map
        std::unordered_map<std::string, std::string> sceneToTmjPath;
        for (const auto& sc : ctx.sceneMap) {
            const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.second.tmjPath);
            if (!tmjPath.empty()) {
                sceneToTmjPath[sc.first] = tmjPath;
            }
        }
        
        const std::string interestZonesPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, "assets/interest_zones.json");
        ctx.graph = dataManager.loadCampusGraph(campusJsonPath, sceneToTmjPath, interestZonesPath, ctx.pixelsToMeters);
        
        const fs::path generatedGraphPath = fs::path(campusJsonPath).parent_path() / "campus.generated.json";
        dataManager.exportResolvedGraph(ctx.graph, generatedGraphPath.string());
    } catch (const std::exception& ex) {
        std::cerr << "Error loading GIS data: " << ex.what() << "\n";
        return false;
    }
    
    ctx.navService = NavigationService(ctx.graph);
    ctx.complexityAnalyzer = ComplexityAnalyzer(ctx.graph);
    ctx.resilienceService = ResilienceService(ctx.graph);
    
    return true;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1183-1210 (loadSceneData)
// ============================================================================

void RuntimeInitManager::loadSceneData(GameContext& ctx, const std::vector<SceneConfig>& allScenes,
                                        const std::string& interestZonesPath, int argc, char* argv[]) {
    const auto interestZonesByScene = InterestZoneLoader::loadFromJson(interestZonesPath);
    
    for (const auto& sc : allScenes) {
        SceneData sd;
        const std::string sceneKey = StringUtils::toLowerCopy(sc.name);
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        
        if (!tmjPath.empty()) {
            try {
                sd.hitboxes = loadHitboxesFromTmj(tmjPath);
                sd.isValid = true;
            } catch (const std::exception& ex) {
                std::cerr << "Could not read " << sc.tmjPath << ": " << ex.what() << "\n";
            }
        } else {
            std::cerr << "Could not find " << sc.tmjPath << "\n";
        }
        
        const auto zoneIt = interestZonesByScene.find(sceneKey);
        if (zoneIt != interestZonesByScene.end()) {
            sd.interestZones = zoneIt->second;
        }
        ctx.sceneDataMap[sc.name] = std::move(sd);
    }
    
    // Load destination catalog
    std::string generatedGraphRuntimePath = (fs::path(allScenes[0].tmjPath).parent_path().parent_path().parent_path() / "campus.generated.json").string();
    if (!fs::exists(generatedGraphRuntimePath)) {
        const fs::path alternateGeneratedPath = fs::path(generatedGraphRuntimePath).parent_path() / "campus_generated.json";
        if (fs::exists(alternateGeneratedPath)) {
            generatedGraphRuntimePath = alternateGeneratedPath.string();
        }
    }
    ctx.destinationCatalog.loadFromGeneratedJson(generatedGraphRuntimePath, ctx.sceneDataMap);
    ctx.scenarioManager.setReferenceWaypoints(ctx.destinationCatalog.preferredReferenceWaypoints(ctx.graph));
    
    // Build route scenes list
    for (const auto& destination : ctx.destinationCatalog.destinations()) {
        ctx.routeScenes.push_back({destination.nodeId, destination.label});
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1225-1366 (setupTransitions)
// ============================================================================

void RuntimeInitManager::setupTransitions(GameContext& ctx, const std::vector<SceneConfig>& allScenes, int argc, char* argv[]) {
    // Helper lambdas
    auto canonicalSceneId = [](std::string sceneName) -> std::string {
        std::transform(sceneName.begin(), sceneName.end(), sceneName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return sceneName;
    };
    
    // Load spawns from TMJ files
    std::unordered_map<std::string, std::unordered_map<std::string, Vector2>> allSceneSpawns;
    std::unordered_map<std::string, std::vector<TmjFloorTriggerDef>> allFloorTriggers;
    
    for (const auto& sc : allScenes) {
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (tmjPath.empty()) continue;
        
        allSceneSpawns[sc.name] = loadSpawnsFromTmj(tmjPath);
        allFloorTriggers[sc.name] = loadFloorTriggersFromTmj(tmjPath);
    }
    
    // Load portals
    for (const auto& sc : allScenes) {
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (tmjPath.empty()) continue;
        
        const auto portalDefs = loadPortalsFromTmj(tmjPath, sc.name);
        for (const auto& pd : portalDefs) {
            const auto targIt = allSceneSpawns.find(pd.toScene);
            if (targIt == allSceneSpawns.end()) {
                std::cerr << "[Portals] Unknown target scene '" << pd.toScene << "' in " << sc.tmjPath << "\n";
                continue;
            }
            const auto spawnIt = targIt->second.find(pd.toSpawnId);
            if (spawnIt == targIt->second.end()) {
                std::cerr << "[Portals] Unknown spawn '" << pd.toSpawnId << "' in scene '" << pd.toScene << "'\n";
                continue;
            }
            
            UniPortal up;
            up.id = pd.portalId;
            up.scene = pd.fromScene;
            up.triggerRect = pd.triggerRect;
            up.targetScene = pd.toScene;
            up.spawnPos = spawnIt->second;
            up.requiresE = pd.requiresE;
            ctx.transitions.addUniPortal(up);
            
            ctx.sceneLinks.push_back({
                up.id, up.scene, up.targetScene,
                up.requiresE ? "Access with E" : "Automatic access",
                up.triggerRect, up.spawnPos, SceneLinkType::Portal
            });
        }
    }
    
    // Load floor elevators
    const std::vector<std::pair<std::string, std::string>> floorScenes = {
        {"piso1", "Floor 1"}, {"piso2", "Floor 2"}, {"piso3", "Floor 3"},
        {"piso4", "Floor 4"}, {"piso5", "Floor 5"}
    };
    
    for (const auto& [sceneName, sceneLabel] : floorScenes) {
        const auto ftIt = allFloorTriggers.find(sceneName);
        if (ftIt == allFloorTriggers.end()) continue;
        
        for (const auto& ft : ftIt->second) {
            std::string destSpawnId;
            SceneLinkType linkType = SceneLinkType::Portal;
            
            if (ft.triggerType == "elevator") { destSpawnId = "elevator_arrive"; linkType = SceneLinkType::Elevator; }
            else if (ft.triggerType == "stair_left") { destSpawnId = "stair_left_arrive"; linkType = SceneLinkType::StairLeft; }
            else if (ft.triggerType == "stair_right") { destSpawnId = "stair_right_arrive"; linkType = SceneLinkType::StairRight; }
            else continue;
            
            FloorElevator fe;
            fe.id = ft.triggerType + "_" + sceneName;
            fe.scene = sceneName;
            fe.triggerRect = ft.triggerRect;
            
            for (const auto& [dstScene, dstLabel] : floorScenes) {
                const auto dstSpawnMapIt = allSceneSpawns.find(dstScene);
                if (dstSpawnMapIt == allSceneSpawns.end()) continue;
                const auto spawnPosIt = dstSpawnMapIt->second.find(destSpawnId);
                if (spawnPosIt == dstSpawnMapIt->second.end()) continue;
                
                fe.floors.push_back({dstScene, spawnPosIt->second, dstLabel});
                if (dstScene == sceneName) continue;
                
                std::string accessLabel = "Access";
                if (linkType == SceneLinkType::Elevator) accessLabel = "Elevator";
                if (linkType == SceneLinkType::StairLeft) accessLabel = "Left stair";
                if (linkType == SceneLinkType::StairRight) accessLabel = "Right stair";
                
                ctx.sceneLinks.push_back({
                    fe.id + "_" + dstScene, sceneName, dstScene,
                    accessLabel, fe.triggerRect, spawnPosIt->second, linkType
                });
            }
            
            if (!fe.floors.empty()) ctx.transitions.addFloorElevator(fe);
        }
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1368-1490 (initRuntimeState)
// ============================================================================

void RuntimeInitManager::initRuntimeState(GameContext& ctx, const std::string& initialSceneName,
                                           SpriteAnim& playerAnim, Vector2& playerPos, Camera2D& camera,
                                           int argc, char* argv[]) {
    ctx.currentSceneName = initialSceneName;
    
    // Load initial scene texture
    MapRenderData mapData;
    const auto& initConfig = ctx.sceneMap.at(initialSceneName);
    const std::string pngPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.pngPath);
    if (!pngPath.empty()) {
        mapData.texture = LoadTexture(pngPath.c_str());
        mapData.hasTexture = mapData.texture.id != 0;
    }
    
    // Rebuild blocker options
    ctx.runtimeBlockerService.rebuildOptions(ctx.graph, ctx.destinationCatalog, ctx.sceneLinks);
    
    // Load player sprites
    const std::string idlePath = AssetPathResolver::findPlayerIdleSprite(argc > 0 ? argv[0] : nullptr);
    const std::string walkPath = AssetPathResolver::findPlayerWalkSprite(argc > 0 ? argv[0] : nullptr);
    RuntimeAnimationManager::loadPlayerSprites(playerAnim, idlePath, walkPath);
    
    // Set player spawn position
    const auto scIt = loadSpawnsFromTmj(AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.tmjPath)).find("bus_arrive");
    if (scIt != loadSpawnsFromTmj(AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.tmjPath)).end()) {
        playerPos = scIt->second;
    } else {
        playerPos = WalkablePathService::findSpawnPoint(mapData);
    }
    
    // Initialize camera
    camera = RuntimeCameraManager::createCamera(ctx.screenWidth, ctx.screenHeight, playerPos);
    
    // Initialize tab state
    const std::string generatedGraphPath = (fs::path("campus.json").parent_path() / "campus.generated.json").string();
    ctx.tabState = createTabManagerState(ctx.graph, generatedGraphPath);
}

// ============================================================================
// Unload resources
// ============================================================================

void RuntimeInitManager::unload(GameContext& ctx, Texture2D& mapTexture, SpriteAnim& playerAnim) {
    if (mapTexture.id != 0) UnloadTexture(mapTexture);
    RuntimeAnimationManager::unload(playerAnim);
    rlImGuiShutdown();
    CloseWindow();
}
