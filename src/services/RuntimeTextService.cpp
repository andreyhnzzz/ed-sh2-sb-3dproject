#include "RuntimeTextService.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

std::string RuntimeTextService::formatElapsedTime(float elapsedSeconds) {
    const int totalSeconds = std::max(0, static_cast<int>(std::round(elapsedSeconds)));
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return TextFormat("%02d:%02d", minutes, seconds);
}

std::vector<std::string> RuntimeTextService::buildGraphOverviewLines(const CampusGraph& graph) {
    std::vector<std::string> lines;
    const auto ids = graph.nodeIds();
    lines.reserve(ids.size() * 3);
    lines.push_back(TextFormat("Nodes (%d):", static_cast<int>(ids.size())));
    for (const auto& id : ids) {
        lines.push_back(" - " + id);
    }

    lines.push_back("");
    lines.push_back("Connections (distance):");

    std::unordered_set<std::string> seenUndirected;
    int edgeIndex = 1;
    for (const auto& from : ids) {
        for (const auto& e : graph.edgesFrom(from)) {
            const std::string a = (e.from < e.to) ? e.from : e.to;
            const std::string b = (e.from < e.to) ? e.to : e.from;
            const std::string key = a + "|" + b + "|" + e.type;
            if (!seenUndirected.insert(key).second) continue;

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss.precision(2);
            oss << edgeIndex++ << ". " << a << " <-> " << b
                << " | base=" << e.base_weight
                << " | mr=" << e.mobility_weight;
            lines.push_back(oss.str());
        }
    }

    return lines;
}

std::vector<std::string> RuntimeTextService::buildTraversalDetailLines(
    const TraversalResult& traversal,
    const char* title) {
    std::vector<std::string> lines;
    lines.reserve(traversal.visit_order.size() + 3);
    lines.push_back(TextFormat("%s (visited=%d):", title, traversal.nodes_visited));
    if (traversal.visit_order.empty()) {
        lines.push_back("No traversal data.");
        return lines;
    }

    for (size_t i = 0; i < traversal.visit_order.size(); ++i) {
        const auto& node = traversal.visit_order[i];
        const auto it = traversal.accumulated_dist.find(node);
        const double acc = (it != traversal.accumulated_dist.end()) ? it->second : 0.0;

        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << (i + 1) << ". " << node << " | acc=" << acc;
        lines.push_back(oss.str());
    }

    return lines;
}
