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

class ComplexityAnalyzer {
public:
    explicit ComplexityAnalyzer(const CampusGraph& graph);
    std::vector<AlgorithmStats> analyze(const std::string& startNode, bool mobilityReduced);

private:
    const CampusGraph& graph_;
};
