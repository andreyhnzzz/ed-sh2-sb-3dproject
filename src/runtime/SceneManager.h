#pragma once

#include "core/runtime/SceneRuntimeTypes.h"
#include "services/RuntimeBlockerService.h"
#include "services/TransitionService.h"

#include <raylib.h>

#include <string>
#include <unordered_map>

class SceneManager {
public:
    explicit SceneManager(const char* executablePath = nullptr);

    void loadScene(const std::string& sceneName,
                   const std::unordered_map<std::string, SceneConfig>& sceneConfigs,
                   const std::unordered_map<std::string, SceneData>& sceneDataMap,
                   const RuntimeBlockerService& blockerService);

    void refreshHitboxes(const std::unordered_map<std::string, SceneData>& sceneDataMap,
                         const RuntimeBlockerService& blockerService);

    void updateTransitions(float dt,
                           TransitionService& transitions,
                           const Rectangle& playerCollider,
                           bool uiActive);

    bool applyPendingSwap(TransitionService& transitions,
                          const std::unordered_map<std::string, SceneConfig>& sceneConfigs,
                          const std::unordered_map<std::string, SceneData>& sceneDataMap,
                          const RuntimeBlockerService& blockerService,
                          Vector2& outSpawnPos);

    void unload();

    const std::string& getCurrentSceneName() const;
    const MapRenderData& getMapData() const;
    MapRenderData& getMapData();

private:
    void refreshMapDataFromSceneData(const SceneData& sceneData,
                                     const std::vector<Rectangle>& runtimeBlockers);

    const char* executablePath_{nullptr};
    std::string currentSceneName_;
    MapRenderData mapData_{};
};
