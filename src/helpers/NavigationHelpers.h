#pragma once
// MIGRADO DESDE main.cpp: Líneas ~41-265 (funciones helper estáticas)
// Responsabilidad: Funciones utilitarias de navegación y overlay

#include "../core/graph/CampusGraph.h"
#include "../core/runtime/SceneRuntimeTypes.h"
#include "../services/ScenarioManager.h"
#include "../services/WalkablePathService.h"
#include <vector>
#include <string>
#include <unordered_map>

struct VisualPoiNode {
    std::string sceneId;
    std::string label;
    Vector2 worldPos{0.0f, 0.0f};
};

class NavigationHelpers {
public:
    // MIGRADO DESDE main.cpp:49-53
    static bool isOverlayEdgeAllowed(const Edge& edge, bool mobilityReduced);
    
    // MIGRADO DESDE main.cpp:55-62
    static bool pathContainsDirectedStep(const std::vector<std::string>& path,
                                         const std::string& from,
                                         const std::string& to);
    
    // MIGRADO DESDE main.cpp:64-82
    static std::vector<VisualPoiNode> collectVisualPoiNodes(
        const std::unordered_map<std::string, SceneData>& sceneDataMap);
    
    // MIGRADO DESDE main.cpp:84-99
    static int countProfileDiscardedEdges(const CampusGraph& graph, bool mobilityReduced);
    
    // MIGRADO DESDE main.cpp:101-109
    static std::string buildSelectionCriterion(StudentType studentType, bool mobilityReduced);
    
    // MIGRADO DESDE main.cpp:111-120
    static Vector2 playerFrontAnchor(const Vector2& playerPos, int direction);
    
    // MIGRADO DESDE main.cpp:122-128
    static void refreshSceneHitboxes(MapRenderData& mapData,
                                     const SceneData& sceneData,
                                     const std::vector<Rectangle>& runtimeBlockers);
    
    // MIGRADO DESDE main.cpp:130-144
    static PathResult mergeProfiledSegments(const std::vector<PathResult>& segments);
    
    // MIGRADO DESDE main.cpp:146-161
    static PathResult runProfiledDfsPath(const CampusGraph& graph,
                                         NavigationService& navService,
                                         ScenarioManager& scenarioManager,
                                         const std::string& origin,
                                         const std::string& destination);
    
    // MIGRADO DESDE main.cpp:163-178
    static PathResult runProfiledAlternatePath(const CampusGraph& graph,
                                               ResilienceService& resilienceService,
                                               ScenarioManager& scenarioManager,
                                               const std::string& origin,
                                               const std::string& destination);
    
    // MIGRADO DESDE main.cpp:200-210
    static bool linkMatchesEdgeType(SceneLinkType linkType, const std::string& edgeType);
    
    // MIGRADO DESDE main.cpp:212-229
    static const Edge* findBestEdgeForLink(const CampusGraph& graph,
                                           const std::string& fromSceneId,
                                           const SceneLink& link);
};
