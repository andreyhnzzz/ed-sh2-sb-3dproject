#include "CampusGraph.h"
#include <stdexcept>

void CampusGraph::addNode(const Node& n) {
    nodes_[n.id] = n;
    if (adj_.find(n.id) == adj_.end())
        adj_[n.id] = {};
}

void CampusGraph::addEdge(const Edge& e) {
    adj_[e.from].push_back(e);
    Edge rev = e;
    rev.from = e.to;
    rev.to = e.from;
    adj_[e.to].push_back(rev);
    ++edge_count_;
}

const Node& CampusGraph::getNode(const std::string& id) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end())
        throw std::out_of_range("Node not found: " + id);
    return it->second;
}

const std::vector<Edge>& CampusGraph::edgesFrom(const std::string& id) const {
    auto it = adj_.find(id);
    if (it == adj_.end()) {
        static const std::vector<Edge> empty;
        return empty;
    }
    return it->second;
}

std::vector<std::string> CampusGraph::nodeIds() const {
    std::vector<std::string> ids;
    ids.reserve(nodes_.size());
    for (auto& [k, v] : nodes_) ids.push_back(k);
    return ids;
}

int CampusGraph::nodeCount() const { return static_cast<int>(nodes_.size()); }
int CampusGraph::edgeCount() const { return edge_count_; }
bool CampusGraph::hasNode(const std::string& id) const { return nodes_.count(id) > 0; }

void CampusGraph::setEdgeBlocked(const std::string& from, const std::string& to, bool blocked) {
    for (auto& e : adj_[from]) {
        if (e.to == to) e.currently_blocked = blocked;
    }
    for (auto& e : adj_[to]) {
        if (e.to == from) e.currently_blocked = blocked;
    }
}

void CampusGraph::unblockAllEdges() {
    for (auto& [id, edges] : adj_) {
        for (auto& e : edges) e.currently_blocked = false;
    }
}
