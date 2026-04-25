#include "ScenePlanService.h"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

bool ScenePlanService::isLinkAllowed(const SceneLink& link, bool mobilityReduced) {
    if (!mobilityReduced) return true;
    return link.type != SceneLinkType::StairLeft &&
           link.type != SceneLinkType::StairRight;
}

std::vector<std::string> ScenePlanService::buildScenePlan(
    const std::string& startScene,
    const std::string& goalScene,
    const std::vector<SceneLink>& links,
    bool mobilityReduced) {
    if (startScene.empty() || goalScene.empty()) return {};
    if (startScene == goalScene) return {startScene};

    std::queue<std::string> pending;
    std::unordered_map<std::string, std::string> previous;
    std::unordered_set<std::string> visited;

    pending.push(startScene);
    visited.insert(startScene);

    while (!pending.empty()) {
        const std::string scene = pending.front();
        pending.pop();

        for (const auto& link : links) {
            if (link.fromScene != scene || !isLinkAllowed(link, mobilityReduced)) continue;
            if (!visited.insert(link.toScene).second) continue;
            previous[link.toScene] = scene;

            if (link.toScene == goalScene) {
                std::vector<std::string> plan;
                std::string current = goalScene;
                plan.push_back(current);
                while (current != startScene) {
                    const auto it = previous.find(current);
                    if (it == previous.end()) return {};
                    current = it->second;
                    plan.push_back(current);
                }
                std::reverse(plan.begin(), plan.end());
                return plan;
            }

            pending.push(link.toScene);
        }
    }

    return {};
}
