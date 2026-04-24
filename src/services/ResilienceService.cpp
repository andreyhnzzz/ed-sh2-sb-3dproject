#include "ResilienceService.h"
#include <algorithm>

ResilienceService::ResilienceService(CampusGraph& graph) : graph_(graph) {}

void ResilienceService::blockEdge(const std::string& from, const std::string& to) {
    graph_.setEdgeBlocked(from, to, true);
    auto pair = std::make_pair(from, to);
    if (std::find(blocked_edges_.begin(), blocked_edges_.end(), pair) == blocked_edges_.end())
        blocked_edges_.push_back(pair);
}

void ResilienceService::unblockEdge(const std::string& from, const std::string& to) {
    graph_.setEdgeBlocked(from, to, false);
    blocked_edges_.erase(std::remove(blocked_edges_.begin(), blocked_edges_.end(),
                                      std::make_pair(from, to)), blocked_edges_.end());
}

void ResilienceService::blockNode(const std::string& nodeId) {
    graph_.setNodeBlocked(nodeId, true);
    if (std::find(blocked_nodes_.begin(), blocked_nodes_.end(), nodeId) == blocked_nodes_.end()) {
        blocked_nodes_.push_back(nodeId);
    }
}

void ResilienceService::unblockNode(const std::string& nodeId) {
    graph_.setNodeBlocked(nodeId, false);
    blocked_nodes_.erase(std::remove(blocked_nodes_.begin(), blocked_nodes_.end(), nodeId), blocked_nodes_.end());
}

void ResilienceService::unblockAll() {
    graph_.unblockAllEdges();
    blocked_edges_.clear();
    blocked_nodes_.clear();
}

PathResult ResilienceService::findAlternatePath(const std::string& from, const std::string& to, bool mobilityReduced) {
    return Algorithms::findPath(graph_, from, to, mobilityReduced, false);
}

std::vector<std::pair<std::string,std::string>> ResilienceService::getBlockedEdges() const {
    return blocked_edges_;
}

std::vector<std::string> ResilienceService::getBlockedNodes() const {
    return blocked_nodes_;
}
