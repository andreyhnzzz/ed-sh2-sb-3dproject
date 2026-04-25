#include "NavigationHelpers.h"
#include "../services/NavigationService.h"
#include "../services/ResilienceService.h"
#include "../services/StringUtils.h"
#include <algorithm>
#include <unordered_set>

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~49-53 (isOverlayEdgeAllowed)
// ============================================================================

bool NavigationHelpers::isOverlayEdgeAllowed(const Edge& edge, bool mobilityReduced) {
    if (edge.currently_blocked) return false;
    if (mobilityReduced && edge.blocked_for_mr) return false;
    return true;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~55-62 (pathContainsDirectedStep)
// ============================================================================

bool NavigationHelpers::pathContainsDirectedStep(const std::vector<std::string>& path,
                                                  const std::string& from,
                                                  const std::string& to) {
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i - 1] == from && path[i] == to) return true;
    }
    return false;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~64-82 (collectVisualPoiNodes)
// ============================================================================

std::vector<VisualPoiNode> NavigationHelpers::collectVisualPoiNodes(
    const std::unordered_map<std::string, SceneData>& sceneDataMap) {
    std::vector<VisualPoiNode> pois;
    for (const auto& [sceneName, sceneData] : sceneDataMap) {
        const std::string sceneId = StringUtils::toLowerCopy(sceneName);
        for (const auto& zone : sceneData.interestZones) {
            if (zone.rects.empty()) continue;
            Vector2 center{0.0f, 0.0f};
            for (const auto& rect : zone.rects) {
                center.x += rect.x + rect.width * 0.5f;
                center.y += rect.y + rect.height * 0.5f;
            }
            center.x /= static_cast<float>(zone.rects.size());
            center.y /= static_cast<float>(zone.rects.size());
            pois.push_back({sceneId, zone.name, center});
        }
    }
    return pois;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~84-99 (countProfileDiscardedEdges)
// ============================================================================

int NavigationHelpers::countProfileDiscardedEdges(const CampusGraph& graph, bool mobilityReduced) {
    if (!mobilityReduced) return 0;

    std::unordered_set<std::string> seen;
    int count = 0;
    for (const auto& from : graph.nodeIds()) {
        for (const auto& edge : graph.edgesFrom(from)) {
            const std::string a = (edge.from < edge.to) ? edge.from : edge.to;
            const std::string b = (edge.from < edge.to) ? edge.to : edge.from;
            const std::string key = a + "|" + b + "|" + edge.type;
            if (!seen.insert(key).second) continue;
            if (edge.blocked_for_mr) ++count;
        }
    }
    return count;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~101-109 (buildSelectionCriterion)
// ============================================================================

std::string NavigationHelpers::buildSelectionCriterion(StudentType studentType, bool mobilityReduced) {
    if (studentType == StudentType::DISABLED_STUDENT || mobilityReduced) {
        return "Bloquea todas las escaleras";
    }
    if (studentType == StudentType::NEW_STUDENT) {
        return "Pasa obligatoriamente por al menos un POI";
    }
    return "Ruta mas corta";
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~111-120 (playerFrontAnchor)
// ============================================================================

Vector2 NavigationHelpers::playerFrontAnchor(const Vector2& playerPos, int direction) {
    constexpr float kOffset = 14.0f;
    switch (direction) {
        case 1: return Vector2{playerPos.x - kOffset, playerPos.y - 6.0f};
        case 2: return Vector2{playerPos.x + kOffset, playerPos.y - 6.0f};
        case 3: return Vector2{playerPos.x, playerPos.y - kOffset};
        case 0:
        default: return Vector2{playerPos.x, playerPos.y + kOffset};
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~122-128 (refreshSceneHitboxes)
// ============================================================================

void NavigationHelpers::refreshSceneHitboxes(MapRenderData& mapData,
                                              const SceneData& sceneData,
                                              const std::vector<Rectangle>& runtimeBlockers) {
    mapData.hitboxes = sceneData.hitboxes;
    mapData.hitboxes.insert(mapData.hitboxes.end(), runtimeBlockers.begin(), runtimeBlockers.end());
    mapData.interestZones = sceneData.interestZones;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~130-144 (mergeProfiledSegments)
// ============================================================================

PathResult NavigationHelpers::mergeProfiledSegments(const std::vector<PathResult>& segments) {
    PathResult merged;
    if (segments.empty()) return merged;

    merged.found = true;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        if (!segment.found || segment.path.empty()) return {};

        merged.total_weight += segment.total_weight;
        if (i == 0) merged.path = segment.path;
        else merged.path.insert(merged.path.end(), segment.path.begin() + 1, segment.path.end());
    }
    return merged;
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~146-161 (runProfiledDfsPath)
// ============================================================================

PathResult NavigationHelpers::runProfiledDfsPath(const CampusGraph& graph,
                                                  NavigationService& navService,
                                                  ScenarioManager& scenarioManager,
                                                  const std::string& origin,
                                                  const std::string& destination) {
    const auto waypoints = scenarioManager.applyProfile(graph, origin, destination);
    if (waypoints.size() < 2) return {};

    std::vector<PathResult> segments;
    segments.reserve(waypoints.size() - 1);
    for (size_t i = 1; i < waypoints.size(); ++i) {
        segments.push_back(navService.findPathDfs(waypoints[i - 1], waypoints[i],
                                                  scenarioManager.isMobilityReduced()));
    }
    return mergeProfiledSegments(segments);
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~163-178 (runProfiledAlternatePath)
// ============================================================================

PathResult NavigationHelpers::runProfiledAlternatePath(const CampusGraph& graph,
                                                        ResilienceService& resilienceService,
                                                        ScenarioManager& scenarioManager,
                                                        const std::string& origin,
                                                        const std::string& destination) {
    const auto waypoints = scenarioManager.applyProfile(graph, origin, destination);
    if (waypoints.size() < 2) return {};

    std::vector<PathResult> segments;
    segments.reserve(waypoints.size() - 1);
    for (size_t i = 1; i < waypoints.size(); ++i) {
        segments.push_back(resilienceService.findAlternatePath(waypoints[i - 1], waypoints[i],
                                                               scenarioManager.isMobilityReduced()));
    }
    return mergeProfiledSegments(segments);
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~200-210 (linkMatchesEdgeType)
// ============================================================================

bool NavigationHelpers::linkMatchesEdgeType(SceneLinkType linkType, const std::string& edgeType) {
    const std::string lowered = StringUtils::toLowerCopy(edgeType);
    switch (linkType) {
        case SceneLinkType::Elevator: return lowered.find("elev") != std::string::npos;
        case SceneLinkType::StairLeft:
        case SceneLinkType::StairRight: return lowered.find("escal") != std::string::npos ||
                                               lowered.find("stair") != std::string::npos;
        case SceneLinkType::Portal:
        default: return lowered.find("portal") != std::string::npos;
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~212-229 (findBestEdgeForLink)
// ============================================================================

const Edge* NavigationHelpers::findBestEdgeForLink(const CampusGraph& graph,
                                                    const std::string& fromSceneId,
                                                    const SceneLink& link) {
    const Edge* best = nullptr;
    for (const auto& edge : graph.edgesFrom(fromSceneId)) {
        if (edge.to != StringUtils::toLowerCopy(link.toScene)) continue;
        if (!linkMatchesEdgeType(link.type, edge.type)) continue;
        if (!best || edge.base_weight < best->base_weight) best = &edge;
    }
    if (best) return best;

    for (const auto& edge : graph.edgesFrom(fromSceneId)) {
        if (edge.to == StringUtils::toLowerCopy(link.toScene)) {
            if (!best || edge.base_weight < best->base_weight) best = &edge;
        }
    }
    return best;
}
