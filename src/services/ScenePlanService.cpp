#include "ScenePlanService.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isStairLink(const SceneLink& link) {
    if (link.type == SceneLinkType::StairLeft || link.type == SceneLinkType::StairRight) {
        return true;
    }

    const std::string idLower = toLower(link.id);
    const std::string labelLower = toLower(link.label);
    return idLower.find("stair") != std::string::npos ||
           idLower.find("escalera") != std::string::npos ||
           labelLower.find("stair") != std::string::npos ||
           labelLower.find("escalera") != std::string::npos;
}
} // namespace

bool ScenePlanService::isLinkAllowed(const SceneLink& link, bool mobilityReduced) {
    if (!mobilityReduced) return true;
    return !isStairLink(link);
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
