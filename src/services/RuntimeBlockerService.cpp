#include "RuntimeBlockerService.h"

#include "StringUtils.h"
#include "WalkablePathService.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool isAccessibilityStairLink(const SceneLink& link) {
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

std::string accessibilityStairEdgeType(const SceneLink& link) {
    if (link.type == SceneLinkType::StairLeft) return "Escalera Izquierda";
    if (link.type == SceneLinkType::StairRight) return "Escalera Derecha";
    return "";
}

Rectangle rectAroundPoint(const Vector2& point, float radius) {
    return Rectangle{point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f};
}

std::string rectKey(const std::string& sceneId, const Rectangle& rect) {
    return toLower(sceneId) + "|" +
           std::to_string(static_cast<int>(std::round(rect.x * 10.0f))) + "|" +
           std::to_string(static_cast<int>(std::round(rect.y * 10.0f))) + "|" +
           std::to_string(static_cast<int>(std::round(rect.width * 10.0f))) + "|" +
           std::to_string(static_cast<int>(std::round(rect.height * 10.0f)));
}
} // namespace

std::string RuntimeBlockerService::edgeKey(const std::string& from,
                                           const std::string& to,
                                           const std::string& type) {
    const std::string a = toLower(from);
    const std::string b = toLower(to);
    if (a < b) return a + "|" + b + "|" + toLower(type);
    return b + "|" + a + "|" + toLower(type);
}

void RuntimeBlockerService::addCollisionRects(const std::string& sceneId,
                                              const std::vector<Rectangle>& rects) {
    if (sceneId.empty() || rects.empty()) return;
    auto& out = sceneCollisionRects_[toLower(sceneId)];
    out.insert(out.end(), rects.begin(), rects.end());
}

void RuntimeBlockerService::rebuildOptions(const CampusGraph& graph,
                                           const DestinationCatalog& catalog,
                                           const std::vector<SceneLink>& sceneLinks) {
    nodeOptions_.clear();
    edgeOptions_.clear();

    for (const auto& destination : catalog.destinations()) {
        if (!graph.hasNode(destination.nodeId)) continue;
        nodeOptions_.push_back(destination);
    }

    std::unordered_set<std::string> seenEdges;
    for (const auto& sceneLink : sceneLinks) {
        const std::string key = edgeKey(sceneLink.fromScene, sceneLink.toScene, sceneLink.label);
        if (!seenEdges.insert(key).second) continue;

        BlockableEdgeOption option;
        option.key = key;
        option.fromNodeId = toLower(sceneLink.fromScene);
        option.toNodeId = toLower(sceneLink.toScene);
        option.type = sceneLink.label;
        option.label = sceneLink.label + ": " + sceneLink.fromScene + " -> " + sceneLink.toScene;
        option.sceneId = toLower(sceneLink.fromScene);
        option.collisionRects.push_back(sceneLink.triggerRect);
        edgeOptions_.push_back(std::move(option));
    }

    for (const auto& poiEdge : catalog.poiEdges()) {
        BlockableEdgeOption option;
        option.key = edgeKey(poiEdge.fromNodeId, poiEdge.toNodeId, "poi");
        option.fromNodeId = poiEdge.fromNodeId;
        option.toNodeId = poiEdge.toNodeId;
        option.type = "POI";
        option.label = "POI: " + poiEdge.label;
        option.sceneId = poiEdge.sceneId;
        option.collisionRects = poiEdge.collisionRects;
        edgeOptions_.push_back(std::move(option));
    }

    std::sort(nodeOptions_.begin(), nodeOptions_.end(), [](const NavigationDestination& a,
                                                           const NavigationDestination& b) {
        return StringUtils::toLowerCopy(a.label) < StringUtils::toLowerCopy(b.label);
    });
    std::sort(edgeOptions_.begin(), edgeOptions_.end(), [](const BlockableEdgeOption& a,
                                                           const BlockableEdgeOption& b) {
        return StringUtils::toLowerCopy(a.label) < StringUtils::toLowerCopy(b.label);
    });
}

bool RuntimeBlockerService::blockNode(const NavigationDestination& destination,
                                      ResilienceService& resilienceService) {
    const std::string canonicalNodeId = toLower(destination.nodeId);
    if (blockedNodeIds_.count(canonicalNodeId) > 0) return false;

    resilienceService.blockNode(canonicalNodeId);
    blockedNodeIds_.insert(canonicalNodeId);

    std::vector<Rectangle> rects = destination.zoneRects;
    if (rects.empty()) {
        rects.push_back(rectAroundPoint(destination.worldPos, destination.isPoi ? 20.0f : 22.0f));
    }
    addCollisionRects(destination.sceneId.empty() ? canonicalNodeId : destination.sceneId, rects);
    return true;
}

bool RuntimeBlockerService::blockEdge(const BlockableEdgeOption& edge,
                                      ResilienceService& resilienceService) {
    if (blockedEdgeKeys_.count(edge.key) > 0) return false;

    resilienceService.blockEdge(edge.fromNodeId, edge.toNodeId, edge.type == "POI" ? "POI" : edge.type);
    blockedEdgeKeys_.insert(edge.key);
    addCollisionRects(edge.sceneId, edge.collisionRects);
    return true;
}

void RuntimeBlockerService::clearAll(ResilienceService& resilienceService) {
    resilienceService.unblockAll();
    blockedNodeIds_.clear();
    blockedEdgeKeys_.clear();
    sceneCollisionRects_.clear();
    accessibilityBlockedEdges_.clear();
    accessibilitySceneCollisionRects_.clear();
    accessibilityStairBlocksEnabled_ = false;
}

void RuntimeBlockerService::setAccessibilityStairBlocks(
    bool enabled,
    ResilienceService& resilienceService,
    const std::vector<SceneLink>& sceneLinks) {
    if (!enabled) {
        for (const auto& blocked : accessibilityBlockedEdges_) {
            resilienceService.unblockEdge(blocked.from, blocked.to, blocked.type);
        }
        accessibilityBlockedEdges_.clear();
        accessibilitySceneCollisionRects_.clear();
        accessibilityStairBlocksEnabled_ = false;
        return;
    }

    if (accessibilityStairBlocksEnabled_) return;

    std::unordered_set<std::string> seenEdges;
    std::unordered_set<std::string> seenRects;
    for (const auto& link : sceneLinks) {
        if (!isAccessibilityStairLink(link)) continue;

        const std::string edgeType = accessibilityStairEdgeType(link);
        if (!edgeType.empty()) {
            const std::string edgeBlockKey = edgeKey(link.fromScene, link.toScene, edgeType);
            if (seenEdges.insert(edgeBlockKey).second) {
                const std::string from = toLower(link.fromScene);
                const std::string to = toLower(link.toScene);
                resilienceService.blockEdge(from, to, edgeType);
                accessibilityBlockedEdges_.push_back({from, to, edgeType});
            }
        }

        const std::string collisionKey = rectKey(link.fromScene, link.triggerRect);
        if (seenRects.insert(collisionKey).second) {
            accessibilitySceneCollisionRects_[toLower(link.fromScene)].push_back(link.triggerRect);
        }
    }

    accessibilityStairBlocksEnabled_ = true;
}

bool RuntimeBlockerService::isNodeBlocked(const std::string& nodeId) const {
    return blockedNodeIds_.count(toLower(nodeId)) > 0;
}

bool RuntimeBlockerService::isEdgeBlocked(const std::string& edgeKeyValue) const {
    return blockedEdgeKeys_.count(edgeKeyValue) > 0;
}

std::vector<Rectangle> RuntimeBlockerService::collisionRectsForScene(const std::string& sceneId) const {
    std::vector<Rectangle> result;
    const auto loweredSceneId = toLower(sceneId);

    const auto it = sceneCollisionRects_.find(loweredSceneId);
    if (it != sceneCollisionRects_.end()) {
        result.insert(result.end(), it->second.begin(), it->second.end());
    }

    const auto accessibilityIt = accessibilitySceneCollisionRects_.find(loweredSceneId);
    if (accessibilityIt != accessibilitySceneCollisionRects_.end()) {
        result.insert(result.end(), accessibilityIt->second.begin(), accessibilityIt->second.end());
    }

    return result;
}
