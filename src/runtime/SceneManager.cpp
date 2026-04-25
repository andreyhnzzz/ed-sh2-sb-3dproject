#include "SceneManager.h"

#include "services/AssetPathResolver.h"

#include <iostream>

SceneManager::SceneManager(const char* executablePath) : executablePath_(executablePath) {}

void SceneManager::loadScene(const std::string& sceneName,
                             const std::unordered_map<std::string, SceneConfig>& sceneConfigs,
                             const std::unordered_map<std::string, SceneData>& sceneDataMap,
                             const RuntimeBlockerService& blockerService) {
    unload();

    const auto configIt = sceneConfigs.find(sceneName);
    if (configIt == sceneConfigs.end()) {
        currentSceneName_ = sceneName;
        return;
    }

    const std::string pngPath =
        AssetPathResolver::resolveAssetPath(executablePath_, configIt->second.pngPath);
    if (!pngPath.empty()) {
        mapData_.texture = LoadTexture(pngPath.c_str());
        mapData_.hasTexture = mapData_.texture.id != 0;
    } else {
        std::cerr << "Could not find " << configIt->second.pngPath << "\n";
    }

    currentSceneName_ = sceneName;
    const auto dataIt = sceneDataMap.find(sceneName);
    if (dataIt != sceneDataMap.end() && dataIt->second.isValid) {
        refreshMapDataFromSceneData(dataIt->second, blockerService.collisionRectsForScene(sceneName));
    }
}

void SceneManager::refreshHitboxes(const std::unordered_map<std::string, SceneData>& sceneDataMap,
                                   const RuntimeBlockerService& blockerService) {
    const auto dataIt = sceneDataMap.find(currentSceneName_);
    if (dataIt == sceneDataMap.end() || !dataIt->second.isValid) return;
    refreshMapDataFromSceneData(dataIt->second, blockerService.collisionRectsForScene(currentSceneName_));
}

void SceneManager::updateTransitions(float dt,
                                     TransitionService& transitions,
                                     const Rectangle& playerCollider,
                                     bool uiActive) {
    if (!uiActive) {
        transitions.update(playerCollider, currentSceneName_, dt);
    }
}

bool SceneManager::applyPendingSwap(TransitionService& transitions,
                                    const std::unordered_map<std::string, SceneConfig>& sceneConfigs,
                                    const std::unordered_map<std::string, SceneData>& sceneDataMap,
                                    const RuntimeBlockerService& blockerService,
                                    Vector2& outSpawnPos) {
    if (!transitions.needsSceneSwap()) return false;

    const TransitionRequest req = transitions.getPendingSwap();
    loadScene(req.targetScene, sceneConfigs, sceneDataMap, blockerService);
    outSpawnPos = req.spawnPos;
    transitions.notifySwapDone();
    return true;
}

void SceneManager::unload() {
    if (mapData_.hasTexture) {
        UnloadTexture(mapData_.texture);
    }
    mapData_.texture = {};
    mapData_.hasTexture = false;
    mapData_.hitboxes.clear();
    mapData_.interestZones.clear();
}

const std::string& SceneManager::getCurrentSceneName() const {
    return currentSceneName_;
}

const MapRenderData& SceneManager::getMapData() const {
    return mapData_;
}

MapRenderData& SceneManager::getMapData() {
    return mapData_;
}

void SceneManager::refreshMapDataFromSceneData(const SceneData& sceneData,
                                               const std::vector<Rectangle>& runtimeBlockers) {
    mapData_.hitboxes = sceneData.hitboxes;
    mapData_.hitboxes.insert(mapData_.hitboxes.end(), runtimeBlockers.begin(), runtimeBlockers.end());
    mapData_.interestZones = sceneData.interestZones;
}
