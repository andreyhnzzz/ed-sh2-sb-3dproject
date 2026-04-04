#include "NavigationService.h"

NavigationService::NavigationService(CampusGraph& graph) : graph_(graph) {}

TraversalResult NavigationService::runDfs(const std::string& start, bool mobilityReduced) const {
    return Algorithms::dfs(graph_, start, mobilityReduced);
}

TraversalResult NavigationService::runBfs(const std::string& start, bool mobilityReduced) const {
    return Algorithms::bfs(graph_, start, mobilityReduced);
}

PathResult NavigationService::findPath(const std::string& from, const std::string& to, bool mobilityReduced) const {
    return Algorithms::findPath(graph_, from, to, mobilityReduced);
}

bool NavigationService::checkConnectivity() const {
    return Algorithms::isConnected(graph_);
}

std::vector<std::vector<std::string>> NavigationService::getComponents() const {
    return Algorithms::findComponents(graph_);
}
