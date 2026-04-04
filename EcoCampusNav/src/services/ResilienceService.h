#pragma once
#include "../core/graph/CampusGraph.h"
#include "../core/graph/Algorithms.h"
#include <string>
#include <vector>
#include <utility>

class ResilienceService {
public:
    explicit ResilienceService(CampusGraph& graph);

    void blockEdge(const std::string& from, const std::string& to);
    void unblockEdge(const std::string& from, const std::string& to);
    void unblockAll();

    PathResult findAlternatePath(const std::string& from, const std::string& to, bool mobilityReduced);
    std::vector<std::pair<std::string,std::string>> getBlockedEdges() const;

private:
    CampusGraph& graph_;
    std::vector<std::pair<std::string,std::string>> blocked_edges_;
};
