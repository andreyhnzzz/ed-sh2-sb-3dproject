#pragma once

#include "core/graph/CampusGraph.h"
#include "core/graph/Algorithms.h"

#include <string>
#include <vector>

class RuntimeTextService {
public:
    static std::string formatElapsedTime(float elapsedSeconds);
    static std::vector<std::string> buildGraphOverviewLines(const CampusGraph& graph);
    static std::vector<std::string> buildTraversalDetailLines(
        const TraversalResult& traversal,
        const char* title);
};
