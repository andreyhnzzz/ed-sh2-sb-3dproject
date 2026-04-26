#pragma once

#include "core/runtime/SceneRuntimeTypes.h"
#include "services/TmjLoader.h"
#include "services/TransitionService.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class FloorLinkLoader {
public:
    static std::vector<SceneLink> loadFloorLinks(
        TransitionService& transitions,
        const std::vector<std::pair<std::string, std::string>>& floorScenes,
        const std::unordered_map<std::string, std::unordered_map<std::string, Vector2>>& allSceneSpawns,
        const std::unordered_map<std::string, std::vector<TmjFloorTriggerDef>>& allFloorTriggers);

private:
    static SceneLink createSceneLink(const std::string& id,
                                     const std::string& fromScene,
                                     const std::string& toScene,
                                     const std::string& label,
                                     const Rectangle& triggerRect,
                                     const Vector2& arrivalSpawn,
                                     SceneLinkType type);
};
