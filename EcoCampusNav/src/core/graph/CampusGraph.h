#pragma once
#include "Node.h"
#include "Edge.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

class CampusGraph {
public:
    void addNode(const Node& n);
    void addEdge(const Edge& e);
    const Node& getNode(const std::string& id) const;
    const std::vector<Edge>& edgesFrom(const std::string& id) const;
    std::vector<std::string> nodeIds() const;
    int nodeCount() const;
    int edgeCount() const;
    bool hasNode(const std::string& id) const;
    void setEdgeBlocked(const std::string& from, const std::string& to, bool blocked);
    void unblockAllEdges();

private:
    std::unordered_map<std::string, Node> nodes_;
    std::unordered_map<std::string, std::vector<Edge>> adj_;
    int edge_count_{0};
};
