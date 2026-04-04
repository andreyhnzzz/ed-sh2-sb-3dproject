#pragma once
#include "CampusGraph.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>

struct TraversalResult {
    std::vector<std::string> visit_order;
    std::unordered_map<std::string, double> accumulated_dist;
    long long elapsed_us{0};
    int nodes_visited{0};
};

struct PathResult {
    std::vector<std::string> path;
    double total_weight{0.0};
    bool found{false};
};

class Algorithms {
public:
    static TraversalResult dfs(const CampusGraph& g, const std::string& start,
                                bool mobility_reduced, bool ignore_currently_blocked = false);
    static TraversalResult bfs(const CampusGraph& g, const std::string& start,
                                bool mobility_reduced, bool ignore_currently_blocked = false);
    static bool isConnected(const CampusGraph& g);
    static std::vector<std::vector<std::string>> findComponents(const CampusGraph& g);
    static PathResult findPath(const CampusGraph& g, const std::string& from,
                                const std::string& to, bool mobility_reduced,
                                bool ignore_currently_blocked = false);

private:
    static bool isAllowed(const Edge& e, bool mobility_reduced, bool ignore_currently_blocked);
    static double effectiveWeight(const Edge& e, bool mobility_reduced);
};
