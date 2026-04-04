#include "ComplexityAnalyzer.h"
#include <sstream>

ComplexityAnalyzer::ComplexityAnalyzer(const CampusGraph& graph) : graph_(graph) {}

std::vector<AlgorithmStats> ComplexityAnalyzer::analyze(const std::string& startNode, bool mobilityReduced) {
    std::vector<AlgorithmStats> results;

    auto dfsResult = Algorithms::dfs(graph_, startNode, mobilityReduced);
    std::ostringstream dfsTheo;
    dfsTheo << "O(V+E) = O(" << graph_.nodeCount() << "+" << graph_.edgeCount() << ")";
    results.push_back({"DFS", dfsResult.nodes_visited, dfsResult.elapsed_us, dfsTheo.str()});

    auto bfsResult = Algorithms::bfs(graph_, startNode, mobilityReduced);
    std::ostringstream bfsTheo;
    bfsTheo << "O(V+E) = O(" << graph_.nodeCount() << "+" << graph_.edgeCount() << ")";
    results.push_back({"BFS", bfsResult.nodes_visited, bfsResult.elapsed_us, bfsTheo.str()});

    return results;
}
