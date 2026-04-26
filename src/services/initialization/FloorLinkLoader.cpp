#include "FloorLinkLoader.h"

#include <string>

std::vector<SceneLink> FloorLinkLoader::loadFloorLinks(
    TransitionService& transitions,
    const std::vector<std::pair<std::string, std::string>>& floorScenes,
    const std::unordered_map<std::string, std::unordered_map<std::string, Vector2>>& allSceneSpawns,
    const std::unordered_map<std::string, std::vector<TmjFloorTriggerDef>>& allFloorTriggers) {
    std::vector<SceneLink> sceneLinks;

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
            if (linkType == SceneLinkType::Elevator) elevator.interactionLabel = "elevator";
            else if (linkType == SceneLinkType::StairLeft) elevator.interactionLabel = "left stair";
            else if (linkType == SceneLinkType::StairRight) elevator.interactionLabel = "right stair";

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

                sceneLinks.push_back(createSceneLink(
                    elevator.id + "_" + dstScene,
                    sceneName,
                    dstScene,
                    accessLabel,
                    trigger.triggerRect,
                    spawnPosIt->second,
                    linkType));
            }

            if (!elevator.floors.empty()) {
                transitions.addFloorElevator(elevator);
            }
        }
    }

    return sceneLinks;
}

SceneLink FloorLinkLoader::createSceneLink(const std::string& id,
                                           const std::string& fromScene,
                                           const std::string& toScene,
                                           const std::string& label,
                                           const Rectangle& triggerRect,
                                           const Vector2& arrivalSpawn,
                                           SceneLinkType type) {
    return SceneLink{id, fromScene, toScene, label, triggerRect, arrivalSpawn, type};
}
