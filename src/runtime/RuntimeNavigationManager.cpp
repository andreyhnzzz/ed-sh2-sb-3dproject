#include "RuntimeNavigationManager.h"

#include "../services/ScenePlanService.h"
#include "../services/StringUtils.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>

namespace {
constexpr float kPixelsToMeters = 0.10f;

std::string canonicalSceneId(std::string sceneName) {
    std::transform(sceneName.begin(), sceneName.end(), sceneName.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return sceneName;
}

double edgeDistanceMeters(const CampusGraph& graph, const std::string& from, const std::string& to) {
    if (!graph.hasNode(from) || !graph.hasNode(to)) return 0.0;
    for (const auto& edge : graph.edgesFrom(from)) {
        if (edge.to == to) return edge.base_weight;
    }
    return 0.0;
}

double calculatePathDistanceMeters(const CampusGraph& graph,
                                   const std::vector<std::string>& pathNodes) {
    if (pathNodes.size() < 2) return 0.0;

    double totalMeters = 0.0;
    for (size_t i = 1; i < pathNodes.size(); ++i) {
        totalMeters += edgeDistanceMeters(graph, pathNodes[i - 1], pathNodes[i]);
    }
    return totalMeters;
}

double calculateRemainingDistanceMeters(const CampusGraph& graph,
                                        const std::vector<std::string>& fullPath,
                                        size_t currentIndex,
                                        double currentLegRemainingMeters) {
    double remaining = std::max(0.0, currentLegRemainingMeters);
    if (fullPath.size() < 2 || currentIndex >= fullPath.size()) return remaining;

    for (size_t i = currentIndex + 1; i + 1 < fullPath.size(); ++i) {
        remaining += edgeDistanceMeters(graph, fullPath[i], fullPath[i + 1]);
    }
    return remaining;
}

bool hasPotentialLoop(const std::vector<std::string>& path) {
    if (path.size() < 4) return false;

    std::unordered_map<std::string, int> visitsByNode;
    for (const auto& nodeId : path) {
        const int visits = ++visitsByNode[nodeId];
        if (visits > 2) return true;
    }

    for (size_t i = 1; i + 1 < path.size(); ++i) {
        if (path[i - 1] == path[i + 1]) return true;
    }

    return false;
}
} // namespace

RuntimeNavigationManager::RuntimeNavigationManager(const DestinationCatalog& catalog)
    : catalog_(catalog) {}

const std::vector<NavigationDestination>& RuntimeNavigationManager::destinations() const {
    return catalog_.destinations();
}

const NavigationDestination* RuntimeNavigationManager::selectedDestination(const RouteRuntimeState& state) const {
    const auto& options = destinations();
    if (options.empty()) return nullptr;
    const int safeIndex = std::clamp(state.selectedDestinationIdx, 0, static_cast<int>(options.size()) - 1);
    return &options[safeIndex];
}

const NavigationDestination* RuntimeNavigationManager::activeDestination(const RouteRuntimeState& state) const {
    if (state.routeTargetNodeId.empty()) return nullptr;
    return catalog_.findDestination(state.routeTargetNodeId);
}

std::string RuntimeNavigationManager::selectedDestinationLabel(const RouteRuntimeState& state) const {
    const NavigationDestination* destination = selectedDestination(state);
    return destination ? destination->label : std::string("(none)");
}

void RuntimeNavigationManager::cycleSelection(RouteRuntimeState& state, int delta) const {
    const auto& options = destinations();
    if (options.empty()) {
        state.selectedDestinationIdx = 0;
        return;
    }
    const int count = static_cast<int>(options.size());
    state.selectedDestinationIdx = (state.selectedDestinationIdx + delta + count) % count;
}

void RuntimeNavigationManager::activateSelectedRoute(RouteRuntimeState& state) const {
    const NavigationDestination* destination = selectedDestination(state);
    if (!destination) return;

    state.routeActive = true;
    state.routeTargetNodeId = destination->nodeId;
    state.routeProgressPct = 0.0f;
    state.routeTotalDistanceMeters = 0.0f;
    state.routeRemainingMeters = 0.0f;
    state.routeTravelElapsed = 0.0f;
    state.routeTravelCompleted = false;
    state.routeLegStartDistance = 0.0f;
    state.routeLegSceneId.clear();
    state.routeLegNextSceneId.clear();
    state.routeScenePlan.clear();
    state.routePathPoints.clear();
    state.routeNextHint.clear();
    state.routeRefreshCooldown = 0.0f;
    state.routePathScene.clear();
}

void RuntimeNavigationManager::clearRoute(RouteRuntimeState& state, bool keepCompletedFlag) const {
    state.routeActive = false;
    if (!keepCompletedFlag) {
        state.routeTargetNodeId.clear();
        state.routeProgressPct = 0.0f;
        state.routeTotalDistanceMeters = 0.0f;
        state.routeRemainingMeters = 0.0f;
        state.routeTravelElapsed = 0.0f;
    }
    if (!keepCompletedFlag) {
        state.routeTravelCompleted = false;
    }
    state.routeLegStartDistance = 0.0f;
    state.routeLegSceneId.clear();
    state.routeLegNextSceneId.clear();
    if (!keepCompletedFlag) {
        state.routeScenePlan.clear();
    }
    state.routePathPoints.clear();
    if (!keepCompletedFlag) {
        state.routeNextHint.clear();
    }
    state.routePathScene.clear();
    state.routeRefreshCooldown = 0.0f;
}

std::string RuntimeNavigationManager::activeDestinationSceneId(const RouteRuntimeState& state) const {
    const NavigationDestination* destination = activeDestination(state);
    return destination ? destination->sceneId : std::string();
}

void RuntimeNavigationManager::refreshRoute(RouteRuntimeState& state,
                                            TabManagerState& tabState,
                                            const CampusGraph& graph,
                                            ScenarioManager& scenarioManager,
                                            ComplexityAnalyzer& complexityAnalyzer,
                                            const std::vector<SceneLink>& sceneLinks,
                                            const MapRenderData& mapData,
                                            const std::string& currentSceneName,
                                            const Vector2& playerPos,
                                            float dt,
                                            const std::function<std::string(const std::string&)>& sceneDisplayName,
                                            const std::function<Vector2(const std::string&)>& sceneTargetPoint) {
    if (state.routeActive && !state.routeTravelCompleted) {
        state.routeTravelElapsed += dt;
    }
    if (!state.routeActive || state.routeTravelCompleted || state.routeTargetNodeId.empty()) return;

    const NavigationDestination* destination = activeDestination(state);
    if (!destination) {
        clearRoute(state);
        return;
    }

    state.routeRefreshCooldown -= dt;
    const bool mobilityChanged = state.routeMobilityReduced != scenarioManager.isMobilityReduced();
    const bool movedEnough = WalkablePathService::distanceBetween(playerPos, state.routeAnchorPos) >= 6.0f;
    const bool sceneChanged = state.routePathScene != currentSceneName;

    if (state.routeRefreshCooldown > 0.0f && !mobilityChanged && !movedEnough && !sceneChanged) {
        return;
    }

    const std::string currentSceneId = canonicalSceneId(currentSceneName);
    state.routeMobilityReduced = scenarioManager.isMobilityReduced();
    state.routeAnchorPos = playerPos;
    state.routePathScene = currentSceneName;
    state.routePathPoints.clear();
    state.routeRefreshCooldown = 0.05f;

    PathResult routedPath =
        scenarioManager.buildProfiledPath(graph, currentSceneId, state.routeTargetNodeId);
    if (routedPath.found && !routedPath.path.empty() && hasPotentialLoop(routedPath.path)) {
        const PathResult directPath =
            Algorithms::findPath(graph, currentSceneId, state.routeTargetNodeId,
                                 scenarioManager.isMobilityReduced(), false);
        if (directPath.found &&
            (directPath.total_weight < routedPath.total_weight * 0.7 ||
             directPath.path.size() < routedPath.path.size())) {
            routedPath = directPath;
        }
    }

    if (routedPath.found && !routedPath.path.empty()) {
        state.routeTotalDistanceMeters =
            static_cast<float>(calculatePathDistanceMeters(graph, routedPath.path));
        if (state.routeScenePlan.empty()) {
            state.routeScenePlan = routedPath.path;
        } else {
            const auto currentInStored =
                std::find(state.routeScenePlan.begin(), state.routeScenePlan.end(), currentSceneId);
            if (currentInStored != state.routeScenePlan.end()) {
                std::vector<std::string> mergedPlan(state.routeScenePlan.begin(),
                                                    std::next(currentInStored));
                mergedPlan.insert(mergedPlan.end(),
                                  std::next(routedPath.path.begin()),
                                  routedPath.path.end());
                state.routeScenePlan = std::move(mergedPlan);
            } else {
                state.routeScenePlan = routedPath.path;
            }
        }
    }

    tabState.lastPath = routedPath;
    tabState.hasPath = routedPath.found;
    tabState.lastAction = "PathDijkstra";
    tabState.lastStats = complexityAnalyzer.analyze(currentSceneId, state.routeMobilityReduced);
    tabState.lastComparison = complexityAnalyzer.compareAlgorithms(
        currentSceneId, state.routeTargetNodeId, state.routeMobilityReduced);
    tabState.hasComparison = true;

    const std::string destinationSceneId = destination->sceneId.empty() ? destination->nodeId : destination->sceneId;
    const bool atDestinationScene = currentSceneId == destinationSceneId;
    const Vector2 destinationPoint = destination->isPoi ? destination->worldPos : sceneTargetPoint(destinationSceneId);
    constexpr float kArrivalThreshold = 18.0f;

    if (atDestinationScene) {
        const float currentToGoal = WalkablePathService::distanceBetween(playerPos, destinationPoint);
        if (state.routeLegSceneId != currentSceneId || state.routeLegNextSceneId != state.routeTargetNodeId ||
            state.routeLegStartDistance <= 0.0f) {
            state.routeLegSceneId = currentSceneId;
            state.routeLegNextSceneId = state.routeTargetNodeId;
            state.routeLegStartDistance = std::max(currentToGoal, 1.0f);
        }

        const float localProgress =
            std::clamp(1.0f - (currentToGoal / state.routeLegStartDistance), 0.0f, 1.0f);
        state.routeProgressPct = std::clamp(localProgress * 100.0f, 0.0f, 100.0f);
        state.routeRemainingMeters = currentToGoal * kPixelsToMeters;
        state.routePathPoints = WalkablePathService::buildWalkablePath(mapData, playerPos, destinationPoint);
        state.routeNextHint = currentToGoal <= kArrivalThreshold
            ? "Destino alcanzado"
            : ("Dirigete a " + destination->label);

        if (currentToGoal <= kArrivalThreshold) {
            state.routeTravelCompleted = true;
            state.routeProgressPct = 100.0f;
            state.routeRemainingMeters = 0.0f;
            state.routeNextHint = "Destino alcanzado";
            tabState.lastPath = {};
            tabState.hasPath = false;
            clearRoute(state, true);
        }
        return;
    }

    if (!routedPath.found || state.routeScenePlan.empty()) {
        state.routeRemainingMeters = 0.0f;
        state.routeNextHint = "No hay conexion disponible";
        return;
    }

    const auto currentIt =
        std::find(state.routeScenePlan.begin(), state.routeScenePlan.end(), currentSceneId);
    const size_t currentIndex =
        currentIt != state.routeScenePlan.end()
            ? static_cast<size_t>(std::distance(state.routeScenePlan.begin(), currentIt))
            : 0U;
    const std::string nextNode =
        (currentIt != state.routeScenePlan.end() && std::next(currentIt) != state.routeScenePlan.end())
            ? *std::next(currentIt)
            : state.routeTargetNodeId;

    if (catalog_.isPoiDestination(nextNode) && catalog_.sceneIdFor(nextNode) == currentSceneId) {
        const Vector2 goal = catalog_.worldPointFor(nextNode);
        state.routePathPoints = WalkablePathService::buildWalkablePath(mapData, playerPos, goal);
        const float currentToGoal = WalkablePathService::distanceBetween(playerPos, goal);
        if (state.routeLegSceneId != currentSceneId || state.routeLegNextSceneId != nextNode ||
            state.routeLegStartDistance <= 0.0f) {
            state.routeLegSceneId = currentSceneId;
            state.routeLegNextSceneId = nextNode;
            state.routeLegStartDistance = std::max(currentToGoal, 1.0f);
        }
        const float localProgress =
            std::clamp(1.0f - (currentToGoal / state.routeLegStartDistance), 0.0f, 1.0f);
        const double legMeters = edgeDistanceMeters(graph, currentSceneId, nextNode);
        const double currentLegRemainingMeters = legMeters * static_cast<double>(1.0f - localProgress);
        state.routeRemainingMeters = static_cast<float>(calculateRemainingDistanceMeters(
            graph, state.routeScenePlan, currentIndex, currentLegRemainingMeters));
        const int totalLegs = std::max(1, static_cast<int>(state.routeScenePlan.size()) - 1);
        const int completedLegs =
            currentIt != state.routeScenePlan.end()
                ? std::max(0, static_cast<int>(std::distance(state.routeScenePlan.begin(), currentIt)))
                : 0;
        const float overallProgress =
            ((static_cast<float>(completedLegs) + localProgress) / static_cast<float>(totalLegs)) * 100.0f;
        state.routeProgressPct = std::max(state.routeProgressPct,
                                          std::clamp(overallProgress, 0.0f, 99.9f));
        state.routeNextHint = "Dirigete a " + catalog_.displayLabel(nextNode);
        return;
    }

    float bestLen = std::numeric_limits<float>::max();
    std::string bestLabel;
    std::vector<Vector2> bestPath;
    for (const auto& link : sceneLinks) {
        if (canonicalSceneId(link.fromScene) != currentSceneId ||
            canonicalSceneId(link.toScene) != nextNode ||
            !ScenePlanService::isLinkAllowed(link, state.routeMobilityReduced)) {
            continue;
        }

        const std::vector<Vector2> candidatePath = WalkablePathService::buildWalkablePath(
            mapData, playerPos, WalkablePathService::rectCenter(link.triggerRect));
        if (candidatePath.empty()) continue;
        const float len = WalkablePathService::polylineLength(candidatePath);
        if (len < bestLen) {
            bestLen = len;
            bestPath = candidatePath;
            bestLabel = link.label;
        }
    }

    state.routePathPoints = std::move(bestPath);
    state.routeNextHint = state.routePathPoints.empty()
        ? "No se pudo trazar la ruta local"
        : "Dirigete a " + bestLabel + " para llegar a " + sceneDisplayName(nextNode);

    const int totalLegs = std::max(1, static_cast<int>(state.routeScenePlan.size()) - 1);
    int completedLegs = 0;
    if (currentIt != state.routeScenePlan.end()) {
        completedLegs = std::max(0, static_cast<int>(std::distance(state.routeScenePlan.begin(), currentIt)));
    }

    float localProgress = 0.0f;
    if (!state.routePathPoints.empty()) {
        const float currentToGoal =
            WalkablePathService::distanceBetween(playerPos, state.routePathPoints.back());
        if (state.routeLegSceneId != currentSceneId || state.routeLegNextSceneId != nextNode ||
            state.routeLegStartDistance <= 0.0f) {
            state.routeLegSceneId = currentSceneId;
            state.routeLegNextSceneId = nextNode;
            state.routeLegStartDistance = std::max(currentToGoal, 1.0f);
        }
        localProgress = std::clamp(1.0f - (currentToGoal / state.routeLegStartDistance), 0.0f, 1.0f);
        const double legMeters = edgeDistanceMeters(graph, currentSceneId, nextNode);
        const double currentLegRemainingMeters = legMeters > 0.0
            ? legMeters * static_cast<double>(1.0f - localProgress)
            : static_cast<double>(currentToGoal * kPixelsToMeters);
        state.routeRemainingMeters = static_cast<float>(calculateRemainingDistanceMeters(
            graph, state.routeScenePlan, currentIndex, currentLegRemainingMeters));
    } else if (currentIt != state.routeScenePlan.end()) {
        state.routeRemainingMeters = static_cast<float>(calculateRemainingDistanceMeters(
            graph, state.routeScenePlan, currentIndex, 0.0));
    }

    const float overallProgress =
        ((static_cast<float>(completedLegs) + localProgress) / static_cast<float>(totalLegs)) * 100.0f;
    state.routeProgressPct = std::max(state.routeProgressPct,
                                      std::clamp(overallProgress, 0.0f, 99.9f));
}

std::vector<Vector2> RuntimeNavigationManager::buildOverlayPathForScene(
    const std::string& currentSceneName,
    const std::vector<std::string>& pathNodes,
    const std::vector<SceneLink>& sceneLinks,
    const MapRenderData& mapData,
    const Vector2& playerPos,
    bool mobilityReduced,
    const std::function<Vector2(const std::string&)>& sceneTargetPoint) const {
    const std::string currentSceneId = canonicalSceneId(currentSceneName);
    if (pathNodes.empty()) return {};

    const auto currentIt = std::find(pathNodes.begin(), pathNodes.end(), currentSceneId);
    if (currentIt == pathNodes.end()) return {};

    if (std::next(currentIt) == pathNodes.end()) {
        return WalkablePathService::buildWalkablePath(
            mapData, playerPos, sceneTargetPoint(currentSceneId));
    }

    const std::string nextNode = *std::next(currentIt);
    if (catalog_.isPoiDestination(nextNode) && catalog_.sceneIdFor(nextNode) == currentSceneId) {
        return WalkablePathService::buildWalkablePath(mapData, playerPos, catalog_.worldPointFor(nextNode));
    }

    float bestLen = std::numeric_limits<float>::max();
    std::vector<Vector2> bestPath;
    for (const auto& link : sceneLinks) {
        if (canonicalSceneId(link.fromScene) != currentSceneId ||
            canonicalSceneId(link.toScene) != nextNode ||
            !ScenePlanService::isLinkAllowed(link, mobilityReduced)) {
            continue;
        }
        const auto candidate = WalkablePathService::buildWalkablePath(
            mapData, playerPos, WalkablePathService::rectCenter(link.triggerRect));
        if (candidate.empty()) continue;

        const float len = WalkablePathService::polylineLength(candidate);
        if (len < bestLen) {
            bestLen = len;
            bestPath = candidate;
        }
    }
    return bestPath;
}
