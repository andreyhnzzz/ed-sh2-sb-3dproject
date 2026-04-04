#include "Algorithms.h"
#include <stack>
#include <queue>
#include <unordered_set>
#include <chrono>

bool Algorithms::isAllowed(const Edge& e, bool mobility_reduced, bool ignore_currently_blocked) {
    if (!ignore_currently_blocked && e.currently_blocked) return false;
    if (mobility_reduced && e.blocked_for_mr) return false;
    return true;
}

double Algorithms::effectiveWeight(const Edge& e, bool mobility_reduced) {
    return mobility_reduced ? e.mobility_weight : e.base_weight;
}

TraversalResult Algorithms::dfs(const CampusGraph& g, const std::string& start,
                                  bool mobility_reduced, bool ignore_currently_blocked) {
    TraversalResult result;
    if (!g.hasNode(start)) return result;

    auto t0 = std::chrono::high_resolution_clock::now();

    std::stack<std::pair<std::string, double>> st;
    st.push({start, 0.0});
    std::unordered_set<std::string> visited;

    while (!st.empty()) {
        auto [node, dist] = st.top(); st.pop();
        if (visited.count(node)) continue;
        visited.insert(node);
        result.visit_order.push_back(node);
        result.accumulated_dist[node] = dist;

        for (auto& edge : g.edgesFrom(node)) {
            if (!visited.count(edge.to) && isAllowed(edge, mobility_reduced, ignore_currently_blocked)) {
                st.push({edge.to, dist + effectiveWeight(edge, mobility_reduced)});
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    result.nodes_visited = static_cast<int>(result.visit_order.size());
    return result;
}

TraversalResult Algorithms::bfs(const CampusGraph& g, const std::string& start,
                                  bool mobility_reduced, bool ignore_currently_blocked) {
    TraversalResult result;
    if (!g.hasNode(start)) return result;

    auto t0 = std::chrono::high_resolution_clock::now();

    std::queue<std::pair<std::string, double>> q;
    std::unordered_map<std::string, double> dist_map;
    q.push({start, 0.0});
    dist_map[start] = 0.0;

    while (!q.empty()) {
        auto [node, dist] = q.front(); q.pop();
        result.visit_order.push_back(node);
        result.accumulated_dist[node] = dist;

        for (auto& edge : g.edgesFrom(node)) {
            if (!dist_map.count(edge.to) && isAllowed(edge, mobility_reduced, ignore_currently_blocked)) {
                dist_map[edge.to] = dist + effectiveWeight(edge, mobility_reduced);
                q.push({edge.to, dist_map[edge.to]});
            }
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    result.nodes_visited = static_cast<int>(result.visit_order.size());
    return result;
}

bool Algorithms::isConnected(const CampusGraph& g) {
    auto ids = g.nodeIds();
    if (ids.empty()) return true;
    auto result = bfs(g, ids[0], false, true);
    return result.nodes_visited == g.nodeCount();
}

std::vector<std::vector<std::string>> Algorithms::findComponents(const CampusGraph& g) {
    std::vector<std::vector<std::string>> components;
    std::unordered_set<std::string> globalVisited;

    for (auto& id : g.nodeIds()) {
        if (globalVisited.count(id)) continue;
        auto result = bfs(g, id, false, true);
        std::vector<std::string> comp;
        for (auto& n : result.visit_order) {
            comp.push_back(n);
            globalVisited.insert(n);
        }
        if (!comp.empty()) components.push_back(comp);
    }
    return components;
}

PathResult Algorithms::findPath(const CampusGraph& g, const std::string& from,
                                  const std::string& to, bool mobility_reduced,
                                  bool ignore_currently_blocked) {
    PathResult result;
    if (!g.hasNode(from) || !g.hasNode(to)) return result;

    std::stack<std::pair<std::string, std::vector<std::string>>> st;
    st.push({from, {from}});
    std::unordered_set<std::string> visited;

    while (!st.empty()) {
        auto [curr, path] = st.top(); st.pop();
        if (curr == to) {
            result.path = path;
            result.found = true;
            double total = 0.0;
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                for (auto& e : g.edgesFrom(path[i])) {
                    if (e.to == path[i + 1]) {
                        total += effectiveWeight(e, mobility_reduced);
                        break;
                    }
                }
            }
            result.total_weight = total;
            return result;
        }
        if (visited.count(curr)) continue;
        visited.insert(curr);

        for (auto& edge : g.edgesFrom(curr)) {
            if (!visited.count(edge.to) && isAllowed(edge, mobility_reduced, ignore_currently_blocked)) {
                auto newPath = path;
                newPath.push_back(edge.to);
                st.push({edge.to, newPath});
            }
        }
    }
    return result;
}
