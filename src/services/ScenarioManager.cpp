#include "ScenarioManager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

ScenarioManager::ScenarioManager() = default;

void ScenarioManager::setMobilityReduced(bool mr) { mobility_reduced_ = mr; }
void ScenarioManager::setStudentType(StudentType st) { student_type_ = st; }
void ScenarioManager::setReferenceWaypoints(std::vector<std::string> referenceWaypoints) {
    reference_waypoints_ = std::move(referenceWaypoints);
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static std::vector<std::string> collectPoiNodeIds(const CampusGraph& graph) {
    std::vector<std::string> poiIds;
    for (const auto& nodeId : graph.nodeIds()) {
        if (!graph.hasNode(nodeId)) continue;
        const auto& node = graph.getNode(nodeId);
        const std::string loweredType = toLower(node.type);
        if (loweredType == "poi" || loweredType.find("poi") != std::string::npos) {
            poiIds.push_back(nodeId);
        }
    }
    return poiIds;
}

static std::string chooseMandatoryWaypoint(const CampusGraph& graph,
                                           const std::vector<std::string>& preferredWaypoints,
                                           const std::string& origin,
                                           const std::string& destination,
                                           bool mobilityReduced) {
    std::vector<std::string> candidates = preferredWaypoints;
    if (candidates.empty()) {
        candidates = collectPoiNodeIds(graph);
    }
    if (candidates.empty()) return "";

    if (std::find(candidates.begin(), candidates.end(), origin) != candidates.end()) return origin;
    if (std::find(candidates.begin(), candidates.end(), destination) != candidates.end()) return destination;

    std::string bestWaypoint;
    double bestCost = std::numeric_limits<double>::infinity();
    for (const auto& waypointId : candidates) {
        if (!graph.hasNode(waypointId) || waypointId == origin || waypointId == destination) continue;
        const PathResult toWaypoint = Algorithms::findPath(graph, origin, waypointId, mobilityReduced, false);
        const PathResult fromWaypoint = Algorithms::findPath(graph, waypointId, destination, mobilityReduced, false);
        if (!toWaypoint.found || !fromWaypoint.found) continue;

        const double candidate = toWaypoint.total_weight + fromWaypoint.total_weight;
        if (candidate < bestCost) {
            bestCost = candidate;
            bestWaypoint = waypointId;
        }
    }

    if (!bestWaypoint.empty()) return bestWaypoint;
    return candidates.front();
}

static bool sameLevel(double a, double b) {
    return std::abs(a - b) < 0.001;
}

static std::string findPoiInSameLevel(const CampusGraph& graph,
                                      const std::vector<std::string>& preferredWaypoints,
                                      const std::string& origin,
                                      const std::string& destination,
                                      double targetZ,
                                      bool mobilityReduced) {
    std::vector<std::string> candidates = preferredWaypoints;
    if (candidates.empty()) {
        candidates = collectPoiNodeIds(graph);
    }

    std::string bestWaypoint;
    double bestCost = std::numeric_limits<double>::infinity();

    for (const auto& waypointId : candidates) {
        if (!graph.hasNode(waypointId) || waypointId == origin || waypointId == destination) continue;

        try {
            const Node& poiNode = graph.getNode(waypointId);
            if (!sameLevel(poiNode.z, targetZ)) continue;

            const PathResult toWaypoint = Algorithms::findPath(graph, origin, waypointId, mobilityReduced, false);
            const PathResult fromWaypoint = Algorithms::findPath(graph, waypointId, destination, mobilityReduced, false);
            if (!toWaypoint.found || !fromWaypoint.found) continue;

            const double candidate = toWaypoint.total_weight + fromWaypoint.total_weight;
            if (candidate < bestCost) {
                bestCost = candidate;
                bestWaypoint = waypointId;
            }
        } catch (...) {
            continue;
        }
    }

    return bestWaypoint;
}

bool ScenarioManager::isMobilityReduced() const {
    return mobility_reduced_ || student_type_ == StudentType::DISABLED_STUDENT;
}

std::vector<std::string> ScenarioManager::applyProfile(const CampusGraph& graph,
                                                       const std::string& origin,
                                                       const std::string& destination) const {
    std::vector<std::string> waypoints;
    if (origin.empty() || destination.empty()) return waypoints;
    waypoints.push_back(origin);

    if (student_type_ == StudentType::NEW_STUDENT) {
        const std::string mandatoryWaypoint = chooseMandatoryWaypoint(
            graph, reference_waypoints_, origin, destination, isMobilityReduced());
        if (!mandatoryWaypoint.empty() &&
            mandatoryWaypoint != origin &&
            mandatoryWaypoint != destination) {
            try {
                const Node& originNode = graph.getNode(origin);
                const Node& destinationNode = graph.getNode(destination);
                const Node& poiNode = graph.getNode(mandatoryWaypoint);

                const bool originAndDestinationShareLevel = sameLevel(originNode.z, destinationNode.z);
                const bool poiSharesLevelWithRoute = sameLevel(poiNode.z, originNode.z);

                if (originAndDestinationShareLevel && !poiSharesLevelWithRoute) {
                    const std::string sameLevelWaypoint = findPoiInSameLevel(
                        graph, reference_waypoints_, origin, destination, originNode.z, isMobilityReduced());
                    if (!sameLevelWaypoint.empty()) {
                        waypoints.push_back(sameLevelWaypoint);
                    } else {
                        waypoints.push_back(mandatoryWaypoint);
                    }
                } else {
                    waypoints.push_back(mandatoryWaypoint);
                }
            } catch (...) {
                waypoints.push_back(mandatoryWaypoint);
            }
        }
    }

    if (waypoints.empty() || waypoints.back() != destination) {
        waypoints.push_back(destination);
    }
    return waypoints;
}

PathResult ScenarioManager::buildProfiledPath(const CampusGraph& graph,
                                              const std::string& origin,
                                              const std::string& destination) const {
    PathResult merged;
    const auto waypoints = applyProfile(graph, origin, destination);
    if (waypoints.size() < 2) return merged;

    merged.found = true;
    for (size_t i = 1; i < waypoints.size(); ++i) {
        const PathResult segment = Algorithms::findPath(
            graph, waypoints[i - 1], waypoints[i], isMobilityReduced(), false);
        if (!segment.found || segment.path.empty()) {
            return {};
        }

        merged.total_weight += segment.total_weight;
        if (merged.path.empty()) {
            merged.path = segment.path;
        } else {
            merged.path.insert(merged.path.end(), segment.path.begin() + 1, segment.path.end());
        }
    }
    return merged;
}
