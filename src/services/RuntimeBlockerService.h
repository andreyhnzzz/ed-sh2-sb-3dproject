#pragma once

#include "DestinationCatalog.h"
#include "ResilienceService.h"
#include "ScenarioManager.h"

#include "../core/runtime/SceneRuntimeTypes.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct BlockableEdgeOption {
    std::string key;
    std::string fromNodeId;
    std::string toNodeId;
    std::string type;
    std::string label;
    std::string sceneId;
    std::vector<Rectangle> collisionRects;
};

class RuntimeBlockerService {
public:
    void rebuildOptions(const CampusGraph& graph,
                        const DestinationCatalog& catalog,
                        const std::vector<SceneLink>& sceneLinks);

    const std::vector<NavigationDestination>& nodeOptions() const { return nodeOptions_; }
    const std::vector<BlockableEdgeOption>& edgeOptions() const { return edgeOptions_; }

    bool blockNode(const NavigationDestination& destination, ResilienceService& resilienceService);
    bool blockEdge(const BlockableEdgeOption& edge, ResilienceService& resilienceService);
    void clearAll(ResilienceService& resilienceService);
    void setAccessibilityStairBlocks(bool enabled,
                                     ResilienceService& resilienceService,
                                     const std::vector<SceneLink>& sceneLinks);
    bool accessibilityStairBlocksEnabled() const { return accessibilityStairBlocksEnabled_; }

    bool isNodeBlocked(const std::string& nodeId) const;
    bool isEdgeBlocked(const std::string& edgeKey) const;
    std::vector<Rectangle> collisionRectsForScene(const std::string& sceneId) const;

private:
    struct AccessibilityBlockedEdge {
        std::string from;
        std::string to;
        std::string type;
    };

    static std::string edgeKey(const std::string& from, const std::string& to, const std::string& type);
    void addCollisionRects(const std::string& sceneId, const std::vector<Rectangle>& rects);

    std::vector<NavigationDestination> nodeOptions_;
    std::vector<BlockableEdgeOption> edgeOptions_;
    std::unordered_set<std::string> blockedNodeIds_;
    std::unordered_set<std::string> blockedEdgeKeys_;
    std::unordered_map<std::string, std::vector<Rectangle>> sceneCollisionRects_;
    bool accessibilityStairBlocksEnabled_{false};
    std::vector<AccessibilityBlockedEdge> accessibilityBlockedEdges_;
    std::unordered_map<std::string, std::vector<Rectangle>> accessibilitySceneCollisionRects_;
};
