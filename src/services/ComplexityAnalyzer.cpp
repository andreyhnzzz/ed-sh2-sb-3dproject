#include "ComplexityAnalyzer.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <queue>
#include <stack>
#include <unordered_set>

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

AlgorithmComparison ComplexityAnalyzer::compareAlgorithms(const std::string& origin,
                                                          const std::string& destination,
                                                          bool mobilityReduced) {
    AlgorithmComparison out;
    if (!graph_.hasNode(origin) || !graph_.hasNode(destination)) return out;

    auto isAllowed = [&](const Edge& e) {
        if (e.currently_blocked) return false;
        if (mobilityReduced && e.blocked_for_mr) return false;
        return true;
    };

    {
        const auto t0 = std::chrono::high_resolution_clock::now();
        std::stack<std::string> pending;
        std::unordered_set<std::string> visited;
        pending.push(origin);
        while (!pending.empty()) {
            const auto node = pending.top();
            pending.pop();
            if (!visited.insert(node).second) continue;
            out.dfs_nodes_visited++;
            if (node == destination) {
                out.dfs_reaches_destination = true;
                break;
            }
            for (const auto& e : graph_.edgesFrom(node)) {
                if (!visited.count(e.to) && isAllowed(e)) pending.push(e.to);
            }
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        out.dfs_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    {
        const auto t0 = std::chrono::high_resolution_clock::now();
        std::queue<std::string> pending;
        std::unordered_set<std::string> visited;
        pending.push(origin);
        visited.insert(origin);
        while (!pending.empty()) {
            const auto node = pending.front();
            pending.pop();
            out.bfs_nodes_visited++;
            if (node == destination) {
                out.bfs_reaches_destination = true;
                break;
            }
            for (const auto& e : graph_.edgesFrom(node)) {
                if (!visited.count(e.to) && isAllowed(e)) {
                    visited.insert(e.to);
                    pending.push(e.to);
                }
            }
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        out.bfs_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    }

    out.delta_elapsed_us = out.bfs_elapsed_us - out.dfs_elapsed_us;
    out.delta_nodes_visited = out.bfs_nodes_visited - out.dfs_nodes_visited;
    return out;
}
