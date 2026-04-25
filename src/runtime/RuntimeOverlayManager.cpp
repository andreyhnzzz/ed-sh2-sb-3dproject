#include "RuntimeOverlayManager.h"
#include "../helpers/NavigationHelpers.h"
#include "../services/StringUtils.h"
#include "../services/WalkablePathService.h"
#include "../services/ScenePlanService.h"
#include <raylib.h>
#include <unordered_set>
#include <algorithm>

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~267-355 (drawCurrentSceneNavigationOverlay)
// ============================================================================

void RuntimeOverlayManager::drawCurrentSceneNavigationOverlay(
    const CampusGraph& graph,
    const std::string& currentSceneName,
    const std::vector<SceneLink>& sceneLinks,
    const DestinationCatalog& destinationCatalog,
    const std::vector<std::string>& activePathNodes,
    const std::vector<std::string>& blockedNodes,
    bool mobilityReduced) {
    
    const std::string currentSceneId = StringUtils::toLowerCopy(currentSceneName);
    if (!graph.hasNode(currentSceneId)) return;

    const Node& sceneNode = graph.getNode(currentSceneId);
    const Vector2 scenePos{static_cast<float>(sceneNode.x), static_cast<float>(sceneNode.y)};
    const bool nodeBlocked =
        std::find(blockedNodes.begin(), blockedNodes.end(), currentSceneId) != blockedNodes.end();

    std::unordered_set<std::string> drawnTargets;
    for (const auto& link : sceneLinks) {
        if (StringUtils::toLowerCopy(link.fromScene) != currentSceneId) continue;

        const Edge* edge = NavigationHelpers::findBestEdgeForLink(graph, currentSceneId, link);
        const std::string dedupeKey = currentSceneId + "|" + StringUtils::toLowerCopy(link.toScene) + "|" +
                                      (edge ? edge->type : std::string("link"));
        if (!drawnTargets.insert(dedupeKey).second) continue;

        const Vector2 targetPos = WalkablePathService::rectCenter(link.triggerRect);
        const bool edgeAllowed = edge ? NavigationHelpers::isOverlayEdgeAllowed(*edge, mobilityReduced) 
                                      : ScenePlanService::isLinkAllowed(link, mobilityReduced);
        const bool onActivePath = NavigationHelpers::pathContainsDirectedStep(activePathNodes, currentSceneId, StringUtils::toLowerCopy(link.toScene));

        const Color edgeColor = onActivePath
            ? Color{70, 210, 255, 255}
            : (edgeAllowed ? Color{170, 205, 255, 200} : Color{220, 90, 90, 180});
        DrawLineEx(scenePos, targetPos, onActivePath ? 4.0f : 2.0f, edgeColor);
        DrawCircleV(targetPos, 6.0f, edgeColor);
        DrawCircleLines(static_cast<int>(targetPos.x), static_cast<int>(targetPos.y), 6.0f, BLACK);

        if (edge) {
            constexpr double kPixelsToMeters = 0.10;
            const double localMeters =
                static_cast<double>(WalkablePathService::distanceBetween(scenePos, targetPos)) * kPixelsToMeters;
            const std::string weightLabel = TextFormat("%.1f m", localMeters);
            const int textX = static_cast<int>((scenePos.x + targetPos.x) * 0.5f);
            const int textY = static_cast<int>((scenePos.y + targetPos.y) * 0.5f) - 18;
            DrawRectangle(textX - 4, textY - 2, MeasureText(weightLabel.c_str(), 12) + 8, 16,
                          Color{0, 0, 0, 175});
            DrawText(weightLabel.c_str(), textX, textY, 12, Color{255, 245, 200, 240});
        }

        const std::string sceneLabel = StringUtils::toLowerCopy(link.toScene);
        DrawText(sceneLabel.c_str(), static_cast<int>(targetPos.x) + 8, static_cast<int>(targetPos.y) - 6,
                 12, Color{240, 245, 255, 220});
    }

    for (const auto& poiEdge : destinationCatalog.poiEdgesForScene(currentSceneId)) {
        const Vector2 poiPos = poiEdge.worldPos;
        const bool onActivePath = NavigationHelpers::pathContainsDirectedStep(activePathNodes, currentSceneId, poiEdge.toNodeId);
        bool edgeAllowed = true;
        for (const auto& edge : graph.edgesFrom(currentSceneId)) {
            if (edge.to == poiEdge.toNodeId) {
                edgeAllowed = NavigationHelpers::isOverlayEdgeAllowed(edge, mobilityReduced);
                break;
            }
        }

        const Color edgeColor = onActivePath
            ? Color{255, 215, 70, 255}
            : (edgeAllowed ? Color{255, 190, 60, 190} : Color{220, 90, 90, 220});
        DrawLineEx(scenePos, poiPos, onActivePath ? 3.0f : 1.5f, edgeColor);
        DrawCircleV(poiPos, 7.0f, edgeColor);
        DrawCircleLines(static_cast<int>(poiPos.x), static_cast<int>(poiPos.y), 7.0f,
                        Color{255, 250, 210, 240});

        const Rectangle titleRect = poiEdge.collisionRects.empty()
            ? Rectangle{poiPos.x, poiPos.y, 0.0f, 0.0f}
            : poiEdge.collisionRects.front();
        const int poiLabelX = static_cast<int>(titleRect.x) + 6;
        const int poiLabelY = static_cast<int>(titleRect.y) + 24;
        DrawText(poiEdge.label.c_str(), poiLabelX, poiLabelY, 12, Color{255, 228, 150, 245});
    }

    const Color nodeColor = nodeBlocked ? Color{230, 90, 90, 240} : Color{85, 160, 255, 240};
    DrawCircleV(scenePos, 10.0f, nodeColor);
    DrawCircleLines(static_cast<int>(scenePos.x), static_cast<int>(scenePos.y), 10.0f, WHITE);
    DrawText(sceneNode.name.c_str(), static_cast<int>(scenePos.x) + 12, static_cast<int>(scenePos.y) - 10,
             14, Color{230, 245, 255, 240});
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~636-708 (drawRaylibNavigationOverlayMenu)
// ============================================================================

void RuntimeOverlayManager::drawRaylibNavigationOverlayMenu(
    bool showNavigationGraph,
    bool infoMenuOpen,
    const std::string& currentSceneName,
    bool mobilityReduced,
    StudentType studentType,
    bool routeActive,
    float routeProgressPct,
    bool resilienceConnected,
    const TabManagerState& state,
    const std::vector<std::string>& blockedNodes) {
    
    if (!showNavigationGraph || infoMenuOpen) return;

    const int x = 16;
    const int y = 64;
    const int w = 440;
    const int h = 230;

    DrawRectangle(x, y, w, h, Color{6, 10, 18, 226});
    DrawRectangleLines(x, y, w, h, Color{70, 120, 200, 235});

    int cy = y + 12;
    DrawText("Navigation Graph", x + 12, cy, 22, Color{235, 242, 255, 245});
    cy += 26;
    DrawLine(x + 10, cy, x + w - 10, cy, Color{56, 92, 150, 220});
    cy += 10;

    DrawText(TextFormat("Scene: %s", currentSceneName.c_str()), x + 12, cy, 21, RAYWHITE); cy += 22;
    
    const char* studentTypeToLabel(StudentType studentType);
    DrawText(TextFormat("Profile: %s", studentTypeToLabel(studentType)), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Mobility reduced: %s", mobilityReduced ? "ON" : "OFF"), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Connectivity: %s", resilienceConnected ? "connected" : "fragmented"),
             x + 12, cy, 21, resilienceConnected ? Color{150, 238, 180, 255} : Color{255, 160, 160, 255}); cy += 22;
    DrawText(TextFormat("Route: %s", routeActive ? "active" : "inactive"), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Route progress: %.1f%%", routeProgressPct), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Blocked nodes: %d", static_cast<int>(blockedNodes.size())), x + 12, cy, 21, Color{255, 224, 170, 255}); cy += 22;
    if (state.hasPath) {
        DrawText(TextFormat("Last path weight: %.2f", state.lastPath.total_weight), x + 12, cy, 21, Color{200, 225, 255, 255});
    }
}

// Helper local (migrado desde main.cpp:678-687)
static const char* studentTypeToLabel(StudentType studentType) {
    switch (studentType) {
        case StudentType::NEW_STUDENT: return "New";
        case StudentType::DISABLED_STUDENT: return "Disabled";
        case StudentType::VETERAN_STUDENT:
        default: return "Veteran";
    }
}
