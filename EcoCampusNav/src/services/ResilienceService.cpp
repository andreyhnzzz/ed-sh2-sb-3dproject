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

void ResilienceService::unblockAll() {
    graph_.unblockAllEdges();
    blocked_edges_.clear();
}

PathResult ResilienceService::findAlternatePath(const std::string& from, const std::string& to, bool mobilityReduced) {
    return Algorithms::findPath(graph_, from, to, mobilityReduced, false);
}

std::vector<std::pair<std::string,std::string>> ResilienceService::getBlockedEdges() const {
    return blocked_edges_;
}
