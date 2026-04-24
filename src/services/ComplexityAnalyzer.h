#pragma once
#include "../core/graph/CampusGraph.h"
#include "../core/graph/Algorithms.h"
#include <string>
#include <vector>

struct AlgorithmStats {
    std::string algorithm;
    int nodes_visited;
    long long elapsed_us;
    std::string theoretical;
};

struct AlgorithmComparison {
    int dfs_nodes_visited{0};
    int bfs_nodes_visited{0};
    long long dfs_elapsed_us{0};
    long long bfs_elapsed_us{0};
    long long delta_elapsed_us{0};
    int delta_nodes_visited{0};
    bool dfs_reaches_destination{false};
    bool bfs_reaches_destination{false};
};

class ComplexityAnalyzer {
public:
    explicit ComplexityAnalyzer(const CampusGraph& graph);
    std::vector<AlgorithmStats> analyze(const std::string& startNode, bool mobilityReduced);
    AlgorithmComparison compareAlgorithms(const std::string& origin,
                                          const std::string& destination,
                                          bool mobilityReduced);

private:
    const CampusGraph& graph_;
};
