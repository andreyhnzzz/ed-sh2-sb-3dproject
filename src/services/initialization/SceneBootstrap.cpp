#include "SceneBootstrap.h"

#include "services/AssetPathResolver.h"
#include "services/InterestZoneLoader.h"
#include "services/StringUtils.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {
std::string canonicalSceneId(std::string sceneName) {
    std::transform(sceneName.begin(), sceneName.end(), sceneName.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return sceneName;
}
}

SceneBootstrap SceneBootstrap::load(const char* executablePath, const std::string& campusJsonPath) {
    SceneBootstrap out;
    out.allScenes = {
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

    out.floorScenes = {
        {"piso1", "Floor 1"},
        {"piso2", "Floor 2"},
        {"piso3", "Floor 3"},
        {"piso4", "Floor 4"},
        {"piso5", "Floor 5"}
    };

    for (const auto& sc : out.allScenes) {
        out.sceneMap[sc.name] = sc;
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(executablePath, sc.tmjPath);
        if (!tmjPath.empty()) {
            out.sceneToTmjPath[sc.name] = tmjPath;
        }
    }

    out.interestZonesPath =
        AssetPathResolver::resolveAssetPath(executablePath, "assets/interest_zones.json");
    const auto interestZonesByScene = InterestZoneLoader::loadFromJson(out.interestZonesPath);

    for (const auto& sc : out.allScenes) {
        SceneData sceneData;
        const std::string sceneKey = StringUtils::toLowerCopy(sc.name);
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(executablePath, sc.tmjPath);
        if (!tmjPath.empty()) {
            try {
                sceneData.hitboxes = loadHitboxesFromTmj(tmjPath);
                sceneData.isValid = true;
                out.allSceneSpawns[sc.name] = loadSpawnsFromTmj(tmjPath);
                out.allFloorTriggers[sc.name] = loadFloorTriggersFromTmj(tmjPath);
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
        out.sceneDataMap[sc.name] = std::move(sceneData);
    }

    out.generatedGraphRuntimePath = (fs::path(campusJsonPath).parent_path() / "campus.generated.json").string();
    if (!fs::exists(out.generatedGraphRuntimePath)) {
        const fs::path alternateGeneratedPath = fs::path(campusJsonPath).parent_path() / "campus_generated.json";
        if (fs::exists(alternateGeneratedPath)) {
            out.generatedGraphRuntimePath = alternateGeneratedPath.string();
        }
    }

    return out;
}

void SceneBootstrap::buildPortalSceneLinks(TransitionService& transitions,
                                           std::vector<SceneLink>& sceneLinks) const {
    for (const auto& sc : allScenes) {
        const auto tmjPathIt = sceneToTmjPath.find(sc.name);
        if (tmjPathIt == sceneToTmjPath.end()) continue;

        const auto portalDefs = loadPortalsFromTmj(tmjPathIt->second, sc.name);
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
}

void SceneBootstrap::buildRouteScenes(const DestinationCatalog& destinationCatalog,
                                      std::vector<std::pair<std::string, std::string>>& routeScenes) const {
    routeScenes.clear();
    for (const auto& destination : destinationCatalog.destinations()) {
        routeScenes.push_back({destination.nodeId, destination.label});
    }
}

std::string SceneBootstrap::resolveSceneName(const std::string& sceneId) const {
    const std::string canonical = canonicalSceneId(sceneId);
    for (const auto& [actualName, _] : sceneMap) {
        if (canonicalSceneId(actualName) == canonical) return actualName;
    }
    return sceneId;
}

std::string SceneBootstrap::sceneDisplayName(const std::string& sceneName,
                                             const DestinationCatalog& destinationCatalog) const {
    const std::string canonical = canonicalSceneId(sceneName);
    if (const NavigationDestination* destination = destinationCatalog.findDestination(canonical)) {
        return destination->label;
    }
    return sceneName;
}

Vector2 SceneBootstrap::sceneTargetPoint(const std::string& sceneName) const {
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
}
