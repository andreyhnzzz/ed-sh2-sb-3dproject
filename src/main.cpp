#include <raylib.h>
#include "rlImGui.h"
#include "imgui.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <limits>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <sstream>
#include "services/NavigationService.h"
#include "services/DataManager.h"
#include "services/ScenarioManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/ResilienceService.h"
#include "services/TransitionService.h"
#include "services/TmjLoader.h"
#include "services/StringUtils.h"
#include "services/InterestZoneLoader.h"
#include "services/AssetPathResolver.h"
#include "services/ScenePlanService.h"
#include "services/WalkablePathService.h"
#include "services/SpriteAnimationService.h"
#include "services/RuntimeTextService.h"
#include "services/MapRenderService.h"
#include "core/runtime/SceneRuntimeTypes.h"
#include "ui/TabManager.h"

namespace fs = std::filesystem;

struct VisualPoiNode {
    std::string sceneId;
    std::string label;
    Vector2 worldPos{0.0f, 0.0f};
};

static bool isOverlayEdgeAllowed(const Edge& edge, bool mobilityReduced) {
    if (edge.currently_blocked) return false;
    if (mobilityReduced && edge.blocked_for_mr) return false;
    return true;
}

static bool pathContainsDirectedStep(const std::vector<std::string>& path,
                                     const std::string& from,
                                     const std::string& to) {
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i - 1] == from && path[i] == to) return true;
    }
    return false;
}

static std::vector<VisualPoiNode> collectVisualPoiNodes(
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

static int countProfileDiscardedEdges(const CampusGraph& graph, bool mobilityReduced) {
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

static std::string buildSelectionCriterion(StudentType studentType, bool mobilityReduced) {
    if (mobilityReduced && studentType == StudentType::NEW_STUDENT) {
        return "Evita gradas y pasa por referencias conocidas";
    }
    if (mobilityReduced) return "Prioriza accesibilidad y evita gradas";
    if (studentType == StudentType::NEW_STUDENT) return "Pasa por referencias conocidas";
    return "Prioriza distancia minima";
}

static PathResult mergeProfiledSegments(const std::vector<PathResult>& segments) {
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

static PathResult runProfiledDfsPath(const CampusGraph& graph,
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

static PathResult runProfiledAlternatePath(const CampusGraph& graph,
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

static bool comboSelectNode(const char* label,
                            const std::vector<std::string>& nodeIds,
                            char* buffer,
                            size_t bufferSize) {
    const char* preview = buffer[0] != '\0' ? buffer : "(select)";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (const auto& nodeId : nodeIds) {
            const bool selected = std::string(buffer) == nodeId;
            if (ImGui::Selectable(nodeId.c_str(), selected)) {
                std::snprintf(buffer, bufferSize, "%s", nodeId.c_str());
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

static bool linkMatchesEdgeType(SceneLinkType linkType, const std::string& edgeType) {
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

static const Edge* findBestEdgeForLink(const CampusGraph& graph,
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

static std::vector<Vector2> buildOverlayPathForScene(const std::string& currentSceneName,
                                                     const std::vector<std::string>& pathNodes,
                                                     const std::vector<SceneLink>& sceneLinks,
                                                     const MapRenderData& mapData,
                                                     const Vector2& playerPos,
                                                     bool mobilityReduced,
                                                     const std::function<Vector2(const std::string&)>& sceneTargetPoint) {
    const std::string currentSceneId = StringUtils::toLowerCopy(currentSceneName);
    if (pathNodes.empty()) return {};

    const auto currentIt = std::find(pathNodes.begin(), pathNodes.end(), currentSceneId);
    if (currentIt == pathNodes.end()) return {};

    if (std::next(currentIt) == pathNodes.end()) {
        return WalkablePathService::buildWalkablePath(mapData, playerPos, sceneTargetPoint(currentSceneId));
    }

    const std::string nextSceneId = *std::next(currentIt);
    float bestLen = std::numeric_limits<float>::max();
    std::vector<Vector2> bestPath;
    for (const auto& link : sceneLinks) {
        if (StringUtils::toLowerCopy(link.fromScene) != currentSceneId || StringUtils::toLowerCopy(link.toScene) != nextSceneId) continue;
        if (!ScenePlanService::isLinkAllowed(link, mobilityReduced)) continue;

        const auto candidate = WalkablePathService::buildWalkablePath(mapData, playerPos, WalkablePathService::rectCenter(link.triggerRect));
        if (candidate.empty()) continue;

        const float len = WalkablePathService::polylineLength(candidate);
        if (len < bestLen) {
            bestLen = len;
            bestPath = candidate;
        }
    }
    return bestPath;
}

static void drawCurrentSceneNavigationOverlay(
    const CampusGraph& graph,
    const std::string& currentSceneName,
    const std::vector<SceneLink>& sceneLinks,
    const std::vector<InterestZone>& interestZones,
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

        const Edge* edge = findBestEdgeForLink(graph, currentSceneId, link);
        const std::string dedupeKey = currentSceneId + "|" + StringUtils::toLowerCopy(link.toScene) + "|" +
                                      (edge ? edge->type : std::string("link"));
        if (!drawnTargets.insert(dedupeKey).second) continue;

        const Vector2 targetPos = WalkablePathService::rectCenter(link.triggerRect);
        const bool edgeAllowed = edge ? isOverlayEdgeAllowed(*edge, mobilityReduced) : ScenePlanService::isLinkAllowed(link, mobilityReduced);
        const bool onActivePath = pathContainsDirectedStep(activePathNodes, currentSceneId, StringUtils::toLowerCopy(link.toScene));

        const Color edgeColor = onActivePath
            ? Color{70, 210, 255, 255}
            : (edgeAllowed ? Color{170, 205, 255, 200} : Color{220, 90, 90, 180});
        DrawLineEx(scenePos, targetPos, onActivePath ? 4.0f : 2.0f, edgeColor);
        DrawCircleV(targetPos, 6.0f, edgeColor);
        DrawCircleLines(static_cast<int>(targetPos.x), static_cast<int>(targetPos.y), 6.0f, BLACK);

        if (edge) {
            const std::string weightLabel = TextFormat("%.1f m", edge->base_weight);
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

    for (const auto& zone : interestZones) {
        if (zone.rects.empty()) continue;

        Vector2 poiPos{0.0f, 0.0f};
        for (const auto& rect : zone.rects) {
            poiPos.x += rect.x + rect.width * 0.5f;
            poiPos.y += rect.y + rect.height * 0.5f;
        }
        poiPos.x /= static_cast<float>(zone.rects.size());
        poiPos.y /= static_cast<float>(zone.rects.size());

        DrawLineEx(scenePos, poiPos, 1.5f, Color{255, 190, 60, 145});
        DrawCircleV(poiPos, 7.0f, Color{255, 180, 50, 235});
        DrawCircleLines(static_cast<int>(poiPos.x), static_cast<int>(poiPos.y), 7.0f,
                        Color{255, 250, 210, 240});
        DrawText(zone.name.c_str(), static_cast<int>(poiPos.x) + 8, static_cast<int>(poiPos.y) - 8,
                 12, Color{255, 228, 150, 240});
    }

    const Color nodeColor = nodeBlocked ? Color{230, 90, 90, 240} : Color{85, 160, 255, 240};
    DrawCircleV(scenePos, 10.0f, nodeColor);
    DrawCircleLines(static_cast<int>(scenePos.x), static_cast<int>(scenePos.y), 10.0f, WHITE);
    DrawText(sceneNode.name.c_str(), static_cast<int>(scenePos.x) + 12, static_cast<int>(scenePos.y) - 10,
             14, Color{230, 245, 255, 240});
}

static void renderAcademicRuntimeOverlay(
    bool& showNavigationGraph,
    bool& showInterestZones,
    TabManagerState& tabState,
    NavigationService& navService,
    ScenarioManager& scenarioManager,
    ComplexityAnalyzer& complexityAnalyzer,
    ResilienceService& resilienceService,
    const CampusGraph& graph,
    const std::unordered_map<std::string, SceneData>& sceneDataMap,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName,
    const std::string& currentSceneName,
    bool routeActive,
    const std::vector<std::string>& routeScenePlan) {
    if (!showNavigationGraph) return;

    const auto nodeIds = graph.nodeIds();
    if (nodeIds.empty()) return;

    if (tabState.startId[0] == '\0') std::snprintf(tabState.startId, sizeof(tabState.startId), "%s", nodeIds.front().c_str());
    if (tabState.endId[0] == '\0') std::snprintf(tabState.endId, sizeof(tabState.endId), "%s", nodeIds.front().c_str());
    if (tabState.nodeId[0] == '\0') std::snprintf(tabState.nodeId, sizeof(tabState.nodeId), "%s", nodeIds.front().c_str());

    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::SetNextWindowPos(ImVec2(18.0f, 54.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Runtime Academico", &showNavigationGraph,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Visualizar Grafo de Navegacion");
    ImGui::SameLine();
    ImGui::TextDisabled("(overlay principal)");

    ImGui::Checkbox("Resaltar POIs (TAB)", &showInterestZones);
    bool mobilityReduced = scenarioManager.isMobilityReduced();
    if (ImGui::Checkbox("Escenario movilidad reducida", &mobilityReduced)) {
        scenarioManager.setMobilityReduced(mobilityReduced);
    }

    int studentProfile = scenarioManager.getStudentType() == StudentType::NEW_STUDENT ? 0 : 1;
    if (ImGui::RadioButton("Estudiante nuevo", studentProfile == 0)) {
        scenarioManager.setStudentType(StudentType::NEW_STUDENT);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Estudiante regular", studentProfile == 1)) {
        scenarioManager.setStudentType(StudentType::REGULAR_STUDENT);
    }

    comboSelectNode("Inicio manual DFS/BFS", nodeIds, tabState.startId, sizeof(tabState.startId));
    comboSelectNode("Destino manual", nodeIds, tabState.endId, sizeof(tabState.endId));
    comboSelectNode("Nodo para resiliencia", nodeIds, tabState.nodeId, sizeof(tabState.nodeId));

    if (ImGui::Button("Ejecutar DFS", ImVec2(120, 0))) {
        tabState.lastTraversal = navService.runDfs(tabState.startId, scenarioManager.isMobilityReduced());
        tabState.hasTraversal = true;
        tabState.lastAction = "DFS";
    }
    ImGui::SameLine();
    if (ImGui::Button("Ejecutar BFS", ImVec2(120, 0))) {
        tabState.lastTraversal = navService.runBfs(tabState.startId, scenarioManager.isMobilityReduced());
        tabState.hasTraversal = true;
        tabState.lastAction = "BFS";
    }
    ImGui::SameLine();
    if (ImGui::Button("Verificar Conexidad", ImVec2(170, 0))) {
        tabState.lastConnected = navService.checkConnectivity();
        tabState.lastAction = "Connectivity";
    }

    if (ImGui::Button("Buscar Camino DFS", ImVec2(170, 0))) {
        tabState.lastPath = runProfiledDfsPath(graph, navService, scenarioManager,
                                               tabState.startId, tabState.endId);
        tabState.hasPath = true;
        tabState.lastAction = "PathDFS";
    }
    ImGui::SameLine();
    if (ImGui::Button("Comparar BFS vs DFS", ImVec2(170, 0))) {
        tabState.lastStats = complexityAnalyzer.analyze(tabState.startId, scenarioManager.isMobilityReduced());
        tabState.lastComparison = complexityAnalyzer.compareAlgorithms(
            tabState.startId, tabState.endId, scenarioManager.isMobilityReduced());
        tabState.hasComparison = true;
        tabState.lastAction = "Complexity";
    }
    ImGui::SameLine();
    if (ImGui::Button("Ruta Alterna", ImVec2(120, 0))) {
        tabState.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager,
                                                     tabState.startId, tabState.endId);
        tabState.hasPath = true;
        tabState.lastAction = "AltPath";
    }

    if (ImGui::Button("Bloquear Arista", ImVec2(130, 0))) {
        resilienceService.blockEdge(tabState.startId, tabState.endId);
        tabState.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager,
                                                     tabState.startId, tabState.endId);
        tabState.hasPath = true;
        tabState.lastAction = "BlockEdge";
    }
    ImGui::SameLine();
    if (ImGui::Button("Bloquear Nodo", ImVec2(130, 0))) {
        resilienceService.blockNode(tabState.nodeId);
        tabState.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager,
                                                     tabState.startId, tabState.endId);
        tabState.hasPath = true;
        tabState.lastAction = "BlockNode";
    }
    ImGui::SameLine();
    if (ImGui::Button("Desbloquear Todo", ImVec2(140, 0))) {
        resilienceService.unblockAll();
        tabState.lastAction = "UnblockAll";
    }

    ImGui::Separator();
    ImGui::Text("Panel de explicacion de logica");
    ImGui::Text("Escena actual: %s", sceneDisplayName(StringUtils::toLowerCopy(currentSceneName)).c_str());
    ImGui::Text("Perfil activo: %s",
                scenarioManager.getStudentType() == StudentType::NEW_STUDENT
                    ? (scenarioManager.isMobilityReduced() ? "Nuevo + Movilidad Reducida" : "Nuevo")
                    : (scenarioManager.isMobilityReduced() ? "Regular + Movilidad Reducida" : "Regular"));
    ImGui::TextWrapped("Criterio aplicado: %s",
                       buildSelectionCriterion(scenarioManager.getStudentType(),
                                               scenarioManager.isMobilityReduced()).c_str());
    ImGui::Text("Aristas descartadas por perfil: %d",
                countProfileDiscardedEdges(graph, scenarioManager.isMobilityReduced()));
    ImGui::Text("Peso total de ruta calculada: %.2f m",
                tabState.hasPath ? tabState.lastPath.total_weight : 0.0);

    const auto blockedEdges = resilienceService.getBlockedEdges();
    const auto blockedNodes = resilienceService.getBlockedNodes();
    ImGui::TextColored(tabState.lastConnected ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f)
                                              : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                       "Conectividad global: %s",
                       resilienceService.isStillConnected() ? "Conectado" : "Fragmentado");
    ImGui::Text("Nodos bloqueados: %d | Aristas bloqueadas: %d",
                static_cast<int>(blockedNodes.size()), static_cast<int>(blockedEdges.size()));

    if (tabState.hasTraversal) {
        ImGui::Text("Ultimo recorrido %s: %d nodos, %lld us",
                    tabState.lastAction.c_str(), tabState.lastTraversal.nodes_visited,
                    tabState.lastTraversal.elapsed_us);
    }
    if (tabState.hasPath) {
        ImGui::Text("Ultima ruta %s: %s",
                    tabState.lastAction.c_str(), tabState.lastPath.found ? "encontrada" : "sin ruta");
    }

    if (tabState.hasComparison) {
        ImGui::Separator();
        ImGui::Text("Analisis comparativo de complejidad");
        ImGui::Columns(3, "complexity_columns", false);
        ImGui::Text("Algoritmo"); ImGui::NextColumn();
        ImGui::Text("Nodos"); ImGui::NextColumn();
        ImGui::Text("Tiempo (us)"); ImGui::NextColumn();
        ImGui::Separator();
        for (const auto& stat : tabState.lastStats) {
            ImGui::Text("%s", stat.algorithm.c_str()); ImGui::NextColumn();
            ImGui::Text("%d", stat.nodes_visited); ImGui::NextColumn();
            ImGui::Text("%lld", stat.elapsed_us); ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Text("DFS alcanza destino: %s | BFS alcanza destino: %s",
                    tabState.lastComparison.dfs_reaches_destination ? "si" : "no",
                    tabState.lastComparison.bfs_reaches_destination ? "si" : "no");
    }

    ImGui::Separator();
    ImGui::Text("Grafo visual y POIs");
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = std::max(240.0f, canvasSize.y);
    ImGui::InvisibleButton("graph_canvas", canvasSize);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(12, 20, 34, 210), 8.0f);
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(90, 130, 190, 220), 8.0f);

    std::unordered_map<std::string, std::vector<VisualPoiNode>> poisByScene;
    for (const auto& poi : collectVisualPoiNodes(sceneDataMap)) {
        poisByScene[poi.sceneId].push_back(poi);
    }

    std::vector<std::string> orderedScenes;
    orderedScenes.reserve(routeScenes.size());
    for (const auto& [sceneId, _] : routeScenes) {
        if (graph.hasNode(sceneId)) orderedScenes.push_back(sceneId);
    }
    if (orderedScenes.empty()) orderedScenes = nodeIds;

    const int columns = 3;
    const int rows = std::max(1, static_cast<int>((orderedScenes.size() + columns - 1) / columns));
    const float cellW = canvasSize.x / static_cast<float>(columns);
    const float cellH = canvasSize.y / static_cast<float>(rows);

    std::unordered_map<std::string, ImVec2> nodeScreenPositions;
    for (size_t i = 0; i < orderedScenes.size(); ++i) {
        const int col = static_cast<int>(i % columns);
        const int row = static_cast<int>(i / columns);
        const std::string& sceneId = orderedScenes[i];

        const ImVec2 cellMin{canvasPos.x + col * cellW, canvasPos.y + row * cellH};
        const ImVec2 cellMax{cellMin.x + cellW, cellMin.y + cellH};
        drawList->AddRect(cellMin, cellMax, IM_COL32(45, 70, 105, 160));
        drawList->AddText(ImVec2(cellMin.x + 8.0f, cellMin.y + 6.0f), IM_COL32(220, 235, 255, 255),
                          sceneDisplayName(sceneId).c_str());

        const ImVec2 nodePos{cellMin.x + cellW * 0.5f, cellMin.y + 34.0f};
        nodeScreenPositions[sceneId] = nodePos;
        const bool isCurrentScene = sceneId == StringUtils::toLowerCopy(currentSceneName);
        const bool isBlockedScene =
            std::find(blockedNodes.begin(), blockedNodes.end(), sceneId) != blockedNodes.end();
        const ImU32 nodeColor = isBlockedScene ? IM_COL32(220, 90, 90, 255)
            : (isCurrentScene ? IM_COL32(50, 210, 255, 255) : IM_COL32(90, 155, 255, 255));
        drawList->AddCircleFilled(nodePos, 8.0f, nodeColor);
        drawList->AddCircle(nodePos, 8.0f, IM_COL32(245, 250, 255, 255), 0, 2.0f);
        drawList->AddText(ImVec2(nodePos.x + 10.0f, nodePos.y - 7.0f), IM_COL32(235, 240, 255, 255),
                          graph.getNode(sceneId).name.c_str());

        const auto poiIt = poisByScene.find(sceneId);
        if (poiIt != poisByScene.end()) {
            for (size_t p = 0; p < poiIt->second.size(); ++p) {
                const ImVec2 poiPos{cellMin.x + 26.0f + static_cast<float>((p % 2) * ((cellW - 52.0f) * 0.52f)),
                                    nodePos.y + 34.0f + static_cast<float>(p / 2) * 26.0f};
                drawList->AddLine(nodePos, poiPos, IM_COL32(255, 190, 60, 120), 1.0f);
                drawList->AddCircleFilled(poiPos, 5.5f, IM_COL32(255, 180, 45, 255));
                const std::string poiLabel = poiIt->second[p].label + " [" + sceneDisplayName(sceneId) + "]";
                drawList->AddText(ImVec2(poiPos.x + 8.0f, poiPos.y - 6.0f), IM_COL32(255, 225, 150, 255),
                                  poiLabel.c_str());
            }
        }
    }

    std::unordered_set<std::string> drawnEdges;
    const std::vector<std::string> highlightedPath = routeActive && !routeScenePlan.empty()
        ? routeScenePlan
        : (tabState.hasPath ? tabState.lastPath.path : std::vector<std::string>{});
    for (const auto& from : nodeIds) {
        for (const auto& edge : graph.edgesFrom(from)) {
            const std::string a = (edge.from < edge.to) ? edge.from : edge.to;
            const std::string b = (edge.from < edge.to) ? edge.to : edge.from;
            const std::string key = a + "|" + b + "|" + edge.type;
            if (!drawnEdges.insert(key).second) continue;
            if (!nodeScreenPositions.count(edge.from) || !nodeScreenPositions.count(edge.to)) continue;

            const bool edgeAllowed = isOverlayEdgeAllowed(edge, scenarioManager.isMobilityReduced());
            const bool onActivePath = pathContainsDirectedStep(highlightedPath, edge.from, edge.to) ||
                                      pathContainsDirectedStep(highlightedPath, edge.to, edge.from);
            const ImU32 edgeColor = onActivePath ? IM_COL32(60, 220, 255, 255)
                : (edgeAllowed ? IM_COL32(175, 200, 235, 190) : IM_COL32(210, 90, 90, 180));
            const float thickness = onActivePath ? 3.0f : 1.7f;
            const ImVec2 pa = nodeScreenPositions[edge.from];
            const ImVec2 pb = nodeScreenPositions[edge.to];
            drawList->AddLine(pa, pb, edgeColor, thickness);

            const ImVec2 mid{(pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f};
            const std::string edgeLabel = TextFormat("%.1f m", edge.base_weight);
            drawList->AddRectFilled(ImVec2(mid.x - 22.0f, mid.y - 10.0f),
                                    ImVec2(mid.x + 22.0f, mid.y + 8.0f),
                                    IM_COL32(0, 0, 0, 170), 4.0f);
            drawList->AddText(ImVec2(mid.x - 18.0f, mid.y - 8.0f), IM_COL32(255, 245, 200, 255),
                              edgeLabel.c_str());
        }
    }

    ImGui::End();
}

static void clampCameraTarget(Camera2D& camera, const MapRenderData& mapData, int screenWidth, int screenHeight) {
    if (!mapData.hasTexture) return;
    const float halfViewWidth = (static_cast<float>(screenWidth) * 0.5f) / camera.zoom;
    const float halfViewHeight = (static_cast<float>(screenHeight) * 0.5f) / camera.zoom;
    const float minX = halfViewWidth;
    const float maxX = static_cast<float>(mapData.texture.width) - halfViewWidth;
    const float minY = halfViewHeight;
    const float maxY = static_cast<float>(mapData.texture.height) - halfViewHeight;

    if (minX > maxX) {
        camera.target.x = static_cast<float>(mapData.texture.width) * 0.5f;
    } else {
        camera.target.x = std::clamp(camera.target.x, minX, maxX);
    }
    if (minY > maxY) {
        camera.target.y = static_cast<float>(mapData.texture.height) * 0.5f;
    } else {
        camera.target.y = std::clamp(camera.target.y, minY, maxY);
    }
}

static bool drawRayButton(const Rectangle& r, const char* label, int fontSize,
                          Color base, Color hover, Color active, Color textColor) {
    const Vector2 mouse = GetMousePosition();
    const bool inside = CheckCollisionPointRec(mouse, r);
    const bool pressed = inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const Color c = pressed ? active : (inside ? hover : base);
    DrawRectangleRec(r, c);
    const int tw = MeasureText(label, fontSize);
    DrawText(label,
             static_cast<int>(r.x + (r.width - tw) * 0.5f),
             static_cast<int>(r.y + (r.height - fontSize) * 0.5f),
             fontSize, textColor);
    return pressed;
}

static void drawRaylibInfoMenu(
    bool& isOpen,
    int screenWidth,
    int screenHeight,
    int& selectedRouteSceneIdx,
    bool& routeActive,
    std::string& routeTargetScene,
    float& routeProgressPct,
    float& routeTravelElapsed,
    bool& routeTravelCompleted,
    float& routeLegStartDistance,
    std::string& routeLegSceneId,
    std::string& routeLegNextSceneId,
    std::vector<std::string>& routeScenePlan,
    std::vector<Vector2>& routePathPoints,
    std::string& routeNextHint,
    float& routeRefreshCooldown,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName,
    const CampusGraph& graph,
    const TraversalResult& dfsTraversal,
    const TraversalResult& bfsTraversal,
    int& rubricViewMode,
    int& graphPage,
    int& dfsPage,
    int& bfsPage,
    bool& showNavigationGraph,
    const TabManagerState& state,
    const std::string& currentSceneName,
    bool showHitboxes,
    bool showTriggers,
    bool showInterestZones,
    bool mobilityReduced,
    StudentType studentType,
    const std::vector<std::string>& blockedNodes,
    bool resilienceConnected) {
    if (!isOpen) return;

    const float uiScale = std::clamp(static_cast<float>(screenHeight) / 900.0f, 1.0f, 1.45f);
    const auto px = [uiScale](int base) {
        return std::max(1, static_cast<int>(std::round(static_cast<float>(base) * uiScale)));
    };

    const int titleFont = px(22);
    const int sectionTitleFont = px(24);
    const int bodyFont = px(18);
    const int bodyMutedFont = px(18);
    const int topBarHeight = px(44);
    const int panelMargin = px(16);
    const int sectionPad = px(14);
    const int buttonHeight = px(34);

    const Color panelBg = Color{10, 18, 32, 238};
    const Color border = Color{130, 150, 185, 235};
    const Color white = Color{245, 247, 250, 255};
    const Color muted = Color{175, 188, 210, 255};
    const Color btn = Color{34, 66, 108, 255};
    const Color btnHover = Color{49, 86, 136, 255};
    const Color btnActive = Color{67, 108, 161, 255};

    DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 0});

    const int margin = panelMargin;
    const int topH = topBarHeight;
    Rectangle topBar{0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(topH)};
    DrawRectangleRec(topBar, Color{26, 62, 115, 245});

    DrawText("Information Menu (Raylib)", margin, px(12), titleFont, white);
    Rectangle closeBtn{static_cast<float>(screenWidth - margin - px(24)), static_cast<float>(px(4)),
                       static_cast<float>(px(32)), static_cast<float>(px(32))};
    if (drawRayButton(closeBtn, "X", titleFont, Color{26, 62, 115, 245}, Color{36, 81, 148, 255},
                      Color{60, 95, 155, 255}, white)) {
        isOpen = false;
        return;
    }

    const int contentY = topH + margin;
    const int contentH = screenHeight - contentY - margin;
    const int leftW = static_cast<int>(screenWidth * 0.32f);
    const int rightX = margin + leftW + margin;
    const int rightW = screenWidth - rightX - margin;
    Rectangle leftPanel{static_cast<float>(margin), static_cast<float>(contentY),
                        static_cast<float>(leftW), static_cast<float>(contentH)};
    Rectangle rightPanel{static_cast<float>(rightX), static_cast<float>(contentY),
                         static_cast<float>(rightW), static_cast<float>(contentH)};
    DrawRectangleRec(leftPanel, panelBg);
    DrawRectangleRec(rightPanel, panelBg);
    DrawRectangleLinesEx(leftPanel, 2.0f, border);
    DrawRectangleLinesEx(rightPanel, 2.0f, border);

    int yLeft = contentY + sectionPad;
    DrawText("Route and Navigation", margin + sectionPad, yLeft, sectionTitleFont, white);
    yLeft += px(38);
    DrawLine(margin + px(12), yLeft, margin + leftW - px(12), yLeft, Color{85, 98, 122, 255});
    yLeft += sectionPad;

    Rectangle prevBtn{static_cast<float>(margin + sectionPad), static_cast<float>(yLeft),
                      static_cast<float>(px(36)), static_cast<float>(buttonHeight)};
    Rectangle nextBtn{static_cast<float>(margin + leftW - sectionPad - px(36)), static_cast<float>(yLeft),
                      static_cast<float>(px(36)), static_cast<float>(buttonHeight)};
    Rectangle labelBox{static_cast<float>(margin + sectionPad + px(42)), static_cast<float>(yLeft),
                       static_cast<float>(leftW - (sectionPad * 2 + px(84))), static_cast<float>(buttonHeight)};

    if (drawRayButton(prevBtn, "<", px(20), btn, btnHover, btnActive, white)) {
        selectedRouteSceneIdx = (selectedRouteSceneIdx - 1 + static_cast<int>(routeScenes.size())) %
                                static_cast<int>(routeScenes.size());
    }
    if (drawRayButton(nextBtn, ">", px(20), btn, btnHover, btnActive, white)) {
        selectedRouteSceneIdx = (selectedRouteSceneIdx + 1) % static_cast<int>(routeScenes.size());
    }

    DrawRectangleRec(labelBox, Color{16, 34, 58, 255});
    DrawRectangleLinesEx(labelBox, 1.5f, Color{80, 118, 170, 220});
    const std::string selectedLabel = routeScenes[selectedRouteSceneIdx].second;
    DrawText(selectedLabel.c_str(), static_cast<int>(labelBox.x + px(10)), static_cast<int>(labelBox.y + px(8)), bodyFont, white);
    yLeft += px(46);

    Rectangle drawRouteBtn{static_cast<float>(margin + sectionPad), static_cast<float>(yLeft),
                           static_cast<float>(px(140)), static_cast<float>(buttonHeight)};
    Rectangle clearBtn{static_cast<float>(margin + sectionPad + px(150)), static_cast<float>(yLeft),
                       static_cast<float>(px(110)), static_cast<float>(buttonHeight)};
    if (drawRayButton(drawRouteBtn, "Draw Route", bodyFont, btn, btnHover, btnActive, white)) {
        routeActive = true;
        routeTargetScene = routeScenes[selectedRouteSceneIdx].first;
        routeProgressPct = 0.0f;
        routeTravelElapsed = 0.0f;
        routeTravelCompleted = false;
        routeLegStartDistance = 0.0f;
        routeLegSceneId.clear();
        routeLegNextSceneId.clear();
        routeRefreshCooldown = 0.0f;
    }
    if (drawRayButton(clearBtn, "Clear", bodyFont, btn, btnHover, btnActive, white)) {
        routeActive = false;
        routeTargetScene.clear();
        routeProgressPct = 0.0f;
        routeTravelElapsed = 0.0f;
        routeTravelCompleted = false;
        routeLegStartDistance = 0.0f;
        routeLegSceneId.clear();
        routeLegNextSceneId.clear();
        routeScenePlan.clear();
        routePathPoints.clear();
        routeNextHint.clear();
    }
    yLeft += px(52);

    if (routeActive) {
        DrawText(TextFormat("Destination: %s", sceneDisplayName(routeTargetScene).c_str()),
                 margin + sectionPad, yLeft, bodyFont, white);
        yLeft += px(24);
        DrawText(routeNextHint.c_str(), margin + sectionPad, yLeft, bodyMutedFont, muted);
        yLeft += px(28);
        DrawText("Scene plan:", margin + sectionPad, yLeft, bodyFont, white);
        yLeft += px(22);
        for (const auto& sceneId : routeScenePlan) {
            const std::string item = "- " + sceneDisplayName(sceneId);
            DrawText(item.c_str(), margin + sectionPad + px(6), yLeft, bodyMutedFont, muted);
            yLeft += px(20);
            if (yLeft > contentY + contentH - px(24)) break;
        }
    }

    int yRight = contentY + sectionPad;
    DrawText("Academic Control", rightX + sectionPad, yRight, sectionTitleFont, white);
    yRight += px(38);
    DrawLine(rightX + px(12), yRight, rightX + rightW - px(12), yRight, Color{85, 98, 122, 255});
    yRight += sectionPad;

    DrawText(TextFormat("Current scene: %s", currentSceneName.c_str()), rightX + sectionPad, yRight, bodyFont, white); yRight += px(24);
    DrawText(TextFormat("Hitboxes: %s", showHitboxes ? "ON" : "OFF"), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    DrawText(TextFormat("Triggers: %s", showTriggers ? "ON" : "OFF"), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    DrawText(TextFormat("Interest zones: %s", showInterestZones ? "ON" : "OFF"), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    DrawText(TextFormat("Reduced mobility: %s", mobilityReduced ? "ON" : "OFF"), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    DrawText(TextFormat("Student profile: %s", studentType == StudentType::NEW_STUDENT ? "New" : "Regular"),
             rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(26);

    Rectangle graphToggleBtn{static_cast<float>(rightX + sectionPad), static_cast<float>(yRight),
                             static_cast<float>(px(290)), static_cast<float>(buttonHeight)};
    if (drawRayButton(graphToggleBtn,
                      showNavigationGraph ? "Visualizar Grafo de Navegacion: ON"
                                          : "Visualizar Grafo de Navegacion: OFF",
                      bodyFont, btn, btnHover, btnActive, white)) {
        showNavigationGraph = !showNavigationGraph;
    }
    yRight += px(46);

    const std::string academicOrigin = sceneDisplayName(currentSceneName);
    const std::string academicDestination = (routeActive && !routeTargetScene.empty())
        ? sceneDisplayName(routeTargetScene)
        : (state.endId[0] != '\0' ? sceneDisplayName(state.endId) : std::string("-"));
    DrawText(TextFormat("Origin: %s", academicOrigin.c_str()), rightX + sectionPad, yRight, bodyFont, white); yRight += px(22);
    DrawText(TextFormat("Destination: %s", academicDestination.c_str()), rightX + sectionPad, yRight, bodyFont, white); yRight += px(22);
    DrawText(TextFormat("Resilience node: %s", state.nodeId), rightX + sectionPad, yRight, bodyFont, white); yRight += px(28);

    if (state.hasTraversal) {
        DrawText(TextFormat("Traversal visited nodes: %d", state.lastTraversal.nodes_visited), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
        DrawText(TextFormat("Traversal time: %lld us", state.lastTraversal.elapsed_us), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(24);
    }
    if (state.hasPath) {
        DrawText(TextFormat("Path found: %s", state.lastPath.found ? "yes" : "no"), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
        DrawText(TextFormat("Path weight: %.2f", state.lastPath.total_weight), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(24);
    }
    if (state.hasComparison) {
        DrawText(TextFormat("DFS reaches destination: %s", state.lastComparison.dfs_reaches_destination ? "yes" : "no"),
                 rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
        DrawText(TextFormat("BFS reaches destination: %s", state.lastComparison.bfs_reaches_destination ? "yes" : "no"),
                 rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    }
    DrawText(TextFormat("Global connectivity: %s", resilienceConnected ? "connected" : "fragmented"),
             rightX + sectionPad, yRight, bodyFont, white); yRight += px(24);

    DrawText("Route summary:", rightX + sectionPad, yRight, bodyFont, white); yRight += px(22);
    const char* routeStatus = !routeActive ? "inactive" : (routeTravelCompleted ? "completed" : "in progress");
    DrawText(TextFormat("Status: %s", routeStatus), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    DrawText(TextFormat("Progress: %.1f%%", routeProgressPct),
             rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    const std::string elapsedLabel = RuntimeTextService::formatElapsedTime(routeTravelElapsed);
    DrawText(TextFormat("Elapsed time: %s", elapsedLabel.c_str()),
             rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(26);
    DrawText(TextFormat("Blocked nodes: %d", static_cast<int>(blockedNodes.size())),
             rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(24);

    DrawText("Rubric evidence:", rightX + sectionPad, yRight, bodyFont, white);
    yRight += px(24);

    const int tabBtnW = px(96);
    const int tabBtnH = px(30);
    Rectangle graphTab{static_cast<float>(rightX + sectionPad), static_cast<float>(yRight),
                       static_cast<float>(tabBtnW), static_cast<float>(tabBtnH)};
    Rectangle dfsTab{static_cast<float>(rightX + sectionPad + tabBtnW + px(8)), static_cast<float>(yRight),
                     static_cast<float>(tabBtnW), static_cast<float>(tabBtnH)};
    Rectangle bfsTab{static_cast<float>(rightX + sectionPad + (tabBtnW + px(8)) * 2), static_cast<float>(yRight),
                     static_cast<float>(tabBtnW), static_cast<float>(tabBtnH)};

    if (drawRayButton(graphTab, "Graph", bodyMutedFont,
                      rubricViewMode == 0 ? btnActive : btn,
                      rubricViewMode == 0 ? btnActive : btnHover,
                      btnActive, white)) rubricViewMode = 0;
    if (drawRayButton(dfsTab, "DFS", bodyMutedFont,
                      rubricViewMode == 1 ? btnActive : btn,
                      rubricViewMode == 1 ? btnActive : btnHover,
                      btnActive, white)) rubricViewMode = 1;
    if (drawRayButton(bfsTab, "BFS", bodyMutedFont,
                      rubricViewMode == 2 ? btnActive : btn,
                      rubricViewMode == 2 ? btnActive : btnHover,
                      btnActive, white)) rubricViewMode = 2;
    yRight += tabBtnH + px(10);

    std::vector<std::string> activeLines;
    int* activePage = &graphPage;
    if (rubricViewMode == 0) {
        activeLines = RuntimeTextService::buildGraphOverviewLines(graph);
        activePage = &graphPage;
    } else if (rubricViewMode == 1) {
        activeLines = RuntimeTextService::buildTraversalDetailLines(dfsTraversal, "DFS order + accumulated distance");
        activePage = &dfsPage;
    } else {
        activeLines = RuntimeTextService::buildTraversalDetailLines(bfsTraversal, "BFS order + accumulated distance");
        activePage = &bfsPage;
    }

    const int listBottomY = contentY + contentH - px(58);
    const int lineHeight = px(20);
    const int linesPerPage = std::max(1, (listBottomY - yRight) / lineHeight);
    const int totalPages = std::max(1, static_cast<int>((activeLines.size() + linesPerPage - 1) / linesPerPage));
    *activePage = std::clamp(*activePage, 0, totalPages - 1);
    const int startIdx = (*activePage) * linesPerPage;
    const int endIdx = std::min(static_cast<int>(activeLines.size()), startIdx + linesPerPage);

    for (int i = startIdx; i < endIdx; ++i) {
        DrawText(activeLines[i].c_str(), rightX + sectionPad, yRight, bodyMutedFont, muted);
        yRight += lineHeight;
    }

    Rectangle prevPageBtn{static_cast<float>(rightX + sectionPad), static_cast<float>(contentY + contentH - px(42)),
                          static_cast<float>(px(36)), static_cast<float>(px(30))};
    Rectangle nextPageBtn{static_cast<float>(rightX + sectionPad + px(140)), static_cast<float>(contentY + contentH - px(42)),
                          static_cast<float>(px(36)), static_cast<float>(px(30))};
    if (drawRayButton(prevPageBtn, "<", bodyFont, btn, btnHover, btnActive, white)) {
        *activePage = std::max(0, *activePage - 1);
    }
    if (drawRayButton(nextPageBtn, ">", bodyFont, btn, btnHover, btnActive, white)) {
        *activePage = std::min(totalPages - 1, *activePage + 1);
    }
    DrawText(TextFormat("Page %d/%d", *activePage + 1, totalPages),
             rightX + sectionPad + px(48), contentY + contentH - px(36), bodyMutedFont, white);
}

int main(int argc, char* argv[]) {
    (void)argc;

    std::string path = AssetPathResolver::findCampusJson(argc > 0 ? argv[0] : nullptr);
    if (path.empty()) {
        std::cerr << "campus.json was not found. Place the file in the working directory.\n";
        return 1;
    }

    int screenWidth = 1280;
    int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "EcoCampusNav (Raylib)");
    const int monitor = GetCurrentMonitor();
    screenWidth = GetMonitorWidth(monitor);
    screenHeight = GetMonitorHeight(monitor);
    SetWindowSize(screenWidth, screenHeight);
    ToggleFullscreen();
    SetTargetFPS(60);

    rlImGuiSetup(true);

    // --- Scene definitions (paths only; transitions are handled by TransitionService) ---
    const std::vector<SceneConfig> allScenes = {
        {"Exteriorcafeteria", "assets/maps/Exteriorcafeteria.png", "assets/maps/Exteriorcafeteria.tmj"},
        {"Paradadebus",       "assets/maps/Paradadebus.png",       "assets/maps/Paradadebus.tmj"},
        {"Interiorcafeteria", "assets/maps/Interiorcafeteria.png", "assets/maps/Interiorcafeteria.tmj"},
        {"biblio",            "assets/maps/biblio.png",            "assets/maps/biblio.tmj"},
        {"piso1",             "assets/maps/piso 1.png",            "assets/maps/piso1.tmj"},
        {"piso2",             "assets/maps/piso2.png",             "assets/maps/piso2.tmj"},
        {"piso3",             "assets/maps/piso 3.png",            "assets/maps/piso3.tmj"},
        {"piso4",             "assets/maps/piso 4.png",            "assets/maps/piso 4.tmj"},
        {"piso5",             "assets/maps/piso 5.png",            "assets/maps/piso 5.tmj"},
    };

    std::unordered_map<std::string, SceneConfig> sceneMap;
    for (const auto& sc : allScenes) sceneMap[sc.name] = sc;

    std::unordered_map<std::string, std::string> sceneToTmjPath;
    for (const auto& sc : allScenes) {
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (!tmjPath.empty()) {
            sceneToTmjPath[sc.name] = tmjPath;
        }
    }

    const std::string interestZonesPath =
        AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, "assets/interest_zones.json");
    const auto interestZonesByScene = InterestZoneLoader::loadFromJson(interestZonesPath);

    DataManager dataManager;
    CampusGraph graph;
    try {
        graph = dataManager.loadCampusGraph(path, sceneToTmjPath);
        const fs::path generatedGraphPath = fs::path(path).parent_path() / "campus.generated.json";
        dataManager.exportResolvedGraph(graph, generatedGraphPath.string());
    } catch (const std::exception& ex) {
        std::cerr << "Error loading GIS data: " << ex.what() << "\n";
        return 1;
    }

    NavigationService nav_service(graph);
    ScenarioManager scenario_manager;
    ComplexityAnalyzer complexity_analyzer(graph);
    ResilienceService resilience_service(graph);

    // Pre-load hitboxes for all scenes into sceneDataMap
    std::unordered_map<std::string, SceneData> sceneDataMap;
    // Also collect all spawns and floor triggers from TMJ for transition setup
    std::unordered_map<std::string, std::unordered_map<std::string, Vector2>> allSceneSpawns;
    std::unordered_map<std::string, std::vector<TmjFloorTriggerDef>> allFloorTriggers;
    std::vector<SceneLink> sceneLinks;

    for (const auto& sc : allScenes) {
        SceneData sd;
        const std::string sceneKey = StringUtils::toLowerCopy(sc.name);
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (!tmjPath.empty()) {
            try {
                sd.hitboxes = loadHitboxesFromTmj(tmjPath);
                sd.isValid  = true;
                allSceneSpawns[sc.name]    = loadSpawnsFromTmj(tmjPath);
                allFloorTriggers[sc.name]  = loadFloorTriggersFromTmj(tmjPath);
            } catch (const std::exception& ex) {
                std::cerr << "Could not read " << sc.tmjPath << ": " << ex.what() << "\n";
            }
        } else {
            std::cerr << "Could not find " << sc.tmjPath << "\n";
        }
        const auto zoneIt = interestZonesByScene.find(sceneKey);
        if (zoneIt != interestZonesByScene.end()) {
            sd.interestZones = zoneIt->second;
        }
        sceneDataMap[sc.name] = std::move(sd);
    }

    // -----------------------------------------------------------------------
    // Transitions: load portals from TMJ, resolve spawn positions, register
    // -----------------------------------------------------------------------
    TransitionService transitions;

    for (const auto& sc : allScenes) {
        const std::string tmjPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (tmjPath.empty()) continue;

        const auto portalDefs = loadPortalsFromTmj(tmjPath, sc.name);
        for (const auto& pd : portalDefs) {
            const auto targIt = allSceneSpawns.find(pd.toScene);
            if (targIt == allSceneSpawns.end()) {
                std::cerr << "[Portals] Unknown target scene '" << pd.toScene
                          << "' in " << sc.tmjPath << "\n";
                continue;
            }
            const auto spawnIt = targIt->second.find(pd.toSpawnId);
            if (spawnIt == targIt->second.end()) {
                std::cerr << "[Portals] Unknown spawn '" << pd.toSpawnId
                          << "' in scene '" << pd.toScene << "'\n";
                continue;
            }
            UniPortal up;
            up.id          = pd.portalId;
            up.scene       = pd.fromScene;
            up.triggerRect = pd.triggerRect;
            up.targetScene = pd.toScene;
            up.spawnPos    = spawnIt->second;
            up.requiresE   = pd.requiresE;
            transitions.addUniPortal(up);
            sceneLinks.push_back({
                up.id,
                up.scene,
                up.targetScene,
                up.requiresE ? "Access with E" : "Automatic access",
                up.triggerRect,
                up.spawnPos,
                SceneLinkType::Portal
            });
        }
    }

    // -----------------------------------------------------------------------
    // Floor elevators: load trigger rects from TMJ, build FloorElevator objects
    // -----------------------------------------------------------------------
    // Ordered list of floor scenes for menu display
    const std::vector<std::pair<std::string,std::string>> floorScenes = {
        {"piso1","Floor 1"}, {"piso2","Floor 2"}, {"piso3","Floor 3"},
        {"piso4","Floor 4"}, {"piso5","Floor 5"}
    };
    const std::vector<std::pair<std::string,std::string>> routeScenes = {
        {"paradadebus", "Bus Stop"},
        {"exteriorcafeteria", "Cafeteria Exterior"},
        {"interiorcafeteria", "Cafeteria Interior"},
        {"biblio", "Library"},
        {"piso1", "Floor 1"},
        {"piso2", "Floor 2"},
        {"piso3", "Floor 3"},
        {"piso4", "Floor 4"},
        {"piso5", "Floor 5"}
    };

    auto canonicalSceneId = [](std::string sceneName) -> std::string {
        std::transform(sceneName.begin(), sceneName.end(), sceneName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return sceneName;
    };

    auto resolveSceneName = [&](const std::string& sceneId) -> std::string {
        const std::string canonical = canonicalSceneId(sceneId);
        for (const auto& [actualName, _] : sceneMap) {
            if (canonicalSceneId(actualName) == canonical) return actualName;
        }
        return sceneId;
    };

    auto sceneDisplayName = [&](const std::string& sceneName) -> std::string {
        const std::string canonical = canonicalSceneId(sceneName);
        for (const auto& [id, label] : routeScenes) {
            if (id == canonical) return label;
        }
        return sceneName;
    };

    auto sceneTargetPoint = [&](const std::string& sceneName) -> Vector2 {
        const auto spawnMapIt = allSceneSpawns.find(resolveSceneName(sceneName));
        if (spawnMapIt == allSceneSpawns.end() || spawnMapIt->second.empty()) {
            return Vector2{0.0f, 0.0f};
        }

        const auto& spawnMap = spawnMapIt->second;
        for (const std::string& preferred : {
                 std::string("bus_arrive"),
                 std::string("ext_from_bus"),
                 std::string("intcafe_arrive"),
                 std::string("biblio_main_arrive"),
                 std::string("elevator_arrive"),
                 std::string("piso4_main_L_arrive")
             }) {
            const auto it = spawnMap.find(preferred);
            if (it != spawnMap.end()) return it->second;
        }

        return spawnMap.begin()->second;
    };

    // For each floor scene that has FloorTrigger data, build FloorElevator instances
    for (const auto& [sceneName, sceneLabel] : floorScenes) {
        const auto ftIt = allFloorTriggers.find(sceneName);
        if (ftIt == allFloorTriggers.end()) continue;

        for (const auto& ft : ftIt->second) {
            // Determine which spawn_id to use in destination scenes
            std::string destSpawnId;
            SceneLinkType linkType = SceneLinkType::Portal;
            if      (ft.triggerType == "elevator")    destSpawnId = "elevator_arrive";
            else if (ft.triggerType == "stair_left")  destSpawnId = "stair_left_arrive";
            else if (ft.triggerType == "stair_right") destSpawnId = "stair_right_arrive";
            else continue;
            if      (ft.triggerType == "elevator")    linkType = SceneLinkType::Elevator;
            else if (ft.triggerType == "stair_left")  linkType = SceneLinkType::StairLeft;
            else if (ft.triggerType == "stair_right") linkType = SceneLinkType::StairRight;

            FloorElevator fe;
            fe.id          = ft.triggerType + "_" + sceneName;
            fe.scene       = sceneName;
            fe.triggerRect = ft.triggerRect;

            for (const auto& [dstScene, dstLabel] : floorScenes) {
                const auto dstSpawnMapIt = allSceneSpawns.find(dstScene);
                if (dstSpawnMapIt == allSceneSpawns.end()) continue;
                const auto spawnPosIt = dstSpawnMapIt->second.find(destSpawnId);
                if (spawnPosIt == dstSpawnMapIt->second.end()) continue;
                fe.floors.push_back({dstScene, spawnPosIt->second, dstLabel});
                if (dstScene == sceneName) continue;

                std::string accessLabel = "Access";
                if (linkType == SceneLinkType::Elevator) accessLabel = "Elevator";
                if (linkType == SceneLinkType::StairLeft) accessLabel = "Left stair";
                if (linkType == SceneLinkType::StairRight) accessLabel = "Right stair";

                sceneLinks.push_back({
                    fe.id + "_" + dstScene,
                    sceneName,
                    dstScene,
                    accessLabel,
                    fe.triggerRect,
                    spawnPosIt->second,
                    linkType
                });
            }
            if (!fe.floors.empty()) transitions.addFloorElevator(fe);
        }
    }

    // Load initial scene
    const std::string initialSceneName = "Paradadebus";
    MapRenderData mapData;
    bool showHitboxes = false;
    bool showTriggers = false;
    bool showInterestZones = true;
    {
        const auto& initConfig = sceneMap.at(initialSceneName);
        const std::string pngPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.pngPath);
        if (!pngPath.empty()) {
            mapData.texture = LoadTexture(pngPath.c_str());
            mapData.hasTexture = mapData.texture.id != 0;
        } else {
            std::cerr << "Could not find " << initConfig.pngPath << "\n";
        }
        const auto sdIt = sceneDataMap.find(initialSceneName);
        if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
            mapData.hitboxes = sdIt->second.hitboxes;
            mapData.interestZones = sdIt->second.interestZones;
        }
    }

    SpriteAnim playerAnim;
    const std::string idlePath = AssetPathResolver::findPlayerIdleSprite(argc > 0 ? argv[0] : nullptr);
    const std::string walkPath = AssetPathResolver::findPlayerWalkSprite(argc > 0 ? argv[0] : nullptr);
    if (!idlePath.empty()) {
        playerAnim.idle = LoadTexture(idlePath.c_str());
        playerAnim.hasIdle = playerAnim.idle.id != 0;
    }
    if (!walkPath.empty()) {
        playerAnim.walk = LoadTexture(walkPath.c_str());
        playerAnim.hasWalk = playerAnim.walk.id != 0;
    }

    if (playerAnim.hasIdle) {
        playerAnim.frameHeight = playerAnim.idle.height;
        playerAnim.frameWidth = 16;
        if (playerAnim.idle.width % playerAnim.frameWidth != 0) {
            playerAnim.frameWidth = std::max(1, playerAnim.idle.height);
        }
        playerAnim.idleFrames = std::max(1, playerAnim.idle.width / playerAnim.frameWidth);
    }
    if (playerAnim.hasWalk) {
        if (playerAnim.walk.width % playerAnim.frameWidth != 0) {
            playerAnim.frameWidth = std::max(1, playerAnim.walk.height);
        }
        playerAnim.walkFrames = std::max(1, playerAnim.walk.width / std::max(1, playerAnim.frameWidth));
    }

    Vector2 playerPos = [&]() -> Vector2 {
        // Use the TMJ "bus_arrive" spawn if available, else fall back to hitbox scan
        const auto scIt = allSceneSpawns.find(initialSceneName);
        if (scIt != allSceneSpawns.end()) {
            const auto spIt = scIt->second.find("bus_arrive");
            if (spIt != scIt->second.end()) return spIt->second;
        }
        return WalkablePathService::findSpawnPoint(mapData);
    }();
    const float playerSpeed = 150.0f;
    const float sprintMultiplier = 1.8f;
    const float playerRenderScale = 1.6f;
    playerAnim.direction = 0;
    Camera2D camera{};
    camera.offset = Vector2{screenWidth * 0.5f, screenHeight * 0.5f};
    camera.target = playerPos;
    camera.rotation = 0.0f;
    camera.zoom = 2.2f;
    const float minZoom = 1.2f;
    const float maxZoom = 4.0f;

    // Scene state
    std::string currentSceneName = initialSceneName;

    const std::string generatedGraphPath =
        (fs::path(path).parent_path() / "campus.generated.json").string();
    TabManagerState tabState = createTabManagerState(graph, generatedGraphPath);
    int selectedRouteSceneIdx = 0;
    bool routeActive = false;
    std::string routeTargetScene;
    float routeProgressPct = 0.0f;
    float routeTravelElapsed = 0.0f;
    bool routeTravelCompleted = false;
    float routeLegStartDistance = 0.0f;
    std::string routeLegSceneId;
    std::string routeLegNextSceneId;
    std::vector<std::string> routeScenePlan;
    std::vector<Vector2> routePathPoints;
    std::string routeNextHint;
    std::string routePathScene;
    bool routeMobilityReduced = scenario_manager.isMobilityReduced();
    float routeRefreshCooldown = 0.0f;
    Vector2 routeAnchorPos = playerPos;
    bool infoMenuOpen = false;
    TraversalResult dfsTraversalView;
    TraversalResult bfsTraversalView;
    int rubricViewMode = 0; // 0=Graph, 1=DFS, 2=BFS
    int graphViewPage = 0;
    int dfsViewPage = 0;
    int bfsViewPage = 0;
    float traversalRefreshCooldown = 0.0f;
    bool showNavigationGraph = false;
    std::vector<Vector2> dfsOverlayPathPoints;
    std::vector<Vector2> alternateOverlayPathPoints;
    {
        const std::string traversalStart = canonicalSceneId(currentSceneName);
        dfsTraversalView = nav_service.runDfs(traversalStart, scenario_manager.isMobilityReduced());
        bfsTraversalView = nav_service.runBfs(traversalStart, scenario_manager.isMobilityReduced());
    }

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        if (IsKeyPressed(KEY_M)) {
            infoMenuOpen = !infoMenuOpen;
        }
        if (IsKeyPressed(KEY_TAB)) {
            showInterestZones = !showInterestZones;
        }

        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && !infoMenuOpen) {
            camera.zoom = std::clamp(camera.zoom + wheel * 0.15f, minZoom, maxZoom);
        }

        float moveX = 0.0f;
        float moveY = 0.0f;
        if (!infoMenuOpen) {
            if (IsKeyDown(KEY_W)) moveY -= 1.0f;
            if (IsKeyDown(KEY_S)) moveY += 1.0f;
            if (IsKeyDown(KEY_A)) moveX -= 1.0f;
            if (IsKeyDown(KEY_D)) moveX += 1.0f;
        }

        if (moveX != 0.0f || moveY != 0.0f) {
            const float len = std::sqrt(moveX * moveX + moveY * moveY);
            moveX /= len;
            moveY /= len;
        }

        // Orden real de bloques en este spritesheet (4 direcciones):
        // 0=right, 1=up, 2=left, 3=down
        if (!infoMenuOpen && IsKeyDown(KEY_W)) {
            playerAnim.direction = 1; // up
        } else if (!infoMenuOpen && IsKeyDown(KEY_S)) {
            playerAnim.direction = 3; // down
        } else if (!infoMenuOpen && IsKeyDown(KEY_A)) {
            playerAnim.direction = 2; // left
        } else if (!infoMenuOpen && IsKeyDown(KEY_D)) {
            playerAnim.direction = 0; // right
        }
        const bool sprinting = !infoMenuOpen &&
            (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        const float currentSpeed = playerSpeed * (sprinting ? sprintMultiplier : 1.0f);

        Vector2 candidate = playerPos;
        candidate.x += moveX * currentSpeed * dt;
        if (mapData.hasTexture) {
            candidate.x = std::clamp(candidate.x, 8.0f, static_cast<float>(mapData.texture.width) - 8.0f);
        }
        if (!WalkablePathService::intersectsAny(WalkablePathService::playerColliderAt(candidate), mapData.hitboxes)) {
            playerPos.x = candidate.x;
        }

        candidate = playerPos;
        candidate.y += moveY * currentSpeed * dt;
        if (mapData.hasTexture) {
            candidate.y = std::clamp(candidate.y, 14.0f, static_cast<float>(mapData.texture.height));
        }
        if (!WalkablePathService::intersectsAny(WalkablePathService::playerColliderAt(candidate), mapData.hitboxes)) {
            playerPos.y = candidate.y;
        }
        if (routeActive && !routeTravelCompleted) {
            routeTravelElapsed += dt;
        }
        camera.target = playerPos;
        clampCameraTarget(camera, mapData, screenWidth, screenHeight);

        if (routeActive) {
            routeRefreshCooldown -= dt;
            const bool mobilityChanged = routeMobilityReduced != scenario_manager.isMobilityReduced();
            const bool movedEnough = WalkablePathService::distanceBetween(playerPos, routeAnchorPos) >= 28.0f;
            const bool sceneChanged = routePathScene != currentSceneName;

            if (routeRefreshCooldown <= 0.0f || mobilityChanged || movedEnough || sceneChanged) {
                const std::string currentSceneId = canonicalSceneId(currentSceneName);
                routeMobilityReduced = scenario_manager.isMobilityReduced();
                routeAnchorPos = playerPos;
                routePathScene = currentSceneName;
                routePathPoints.clear();
                const PathResult routedPath =
                    scenario_manager.buildProfiledPath(graph, currentSceneId, routeTargetScene);
                routeScenePlan = routedPath.path;
                routeRefreshCooldown = 0.20f;

                tabState.lastPath = routedPath;
                tabState.hasPath = routedPath.found;
                tabState.lastStats = complexity_analyzer.analyze(currentSceneId, routeMobilityReduced);
                tabState.lastComparison = complexity_analyzer.compareAlgorithms(
                    currentSceneId, routeTargetScene, routeMobilityReduced);
                tabState.hasComparison = true;

                const bool atDestinationScene = currentSceneId == routeTargetScene;
                if (atDestinationScene) {
                    const Vector2 goal = sceneTargetPoint(routeTargetScene);
                    const float currentToGoal = WalkablePathService::distanceBetween(playerPos, goal);
                    if (routeLegSceneId != currentSceneId || routeLegNextSceneId != routeTargetScene ||
                        routeLegStartDistance <= 0.0f) {
                        routeLegSceneId = currentSceneId;
                        routeLegNextSceneId = routeTargetScene;
                        routeLegStartDistance = std::max(currentToGoal, 1.0f);
                    }

                    const float localProgress = std::clamp(1.0f - (currentToGoal / routeLegStartDistance), 0.0f, 1.0f);
                    routeProgressPct = std::max(routeProgressPct, localProgress * 100.0f);
                    routePathPoints = WalkablePathService::buildWalkablePath(mapData, playerPos, goal);
                    routeNextHint = currentToGoal <= 24.0f
                        ? "Destination reached"
                        : "Follow the route to the destination";
                    if (currentToGoal <= 24.0f) {
                        routeTravelCompleted = true;
                        routeProgressPct = 100.0f;
                    }
                } else if (!routedPath.found || routeScenePlan.empty()) {
                    routeNextHint = "No available connection";
                } else {
                    const auto currentIt = std::find(routeScenePlan.begin(), routeScenePlan.end(), currentSceneId);
                    const std::string nextScene = (currentIt != routeScenePlan.end() &&
                                                   std::next(currentIt) != routeScenePlan.end())
                        ? *std::next(currentIt)
                        : routeTargetScene;
                    float bestLen = std::numeric_limits<float>::max();
                    std::string bestLabel;
                    std::vector<Vector2> bestPath;

                    for (const auto& link : sceneLinks) {
                        if (canonicalSceneId(link.fromScene) != currentSceneId ||
                            canonicalSceneId(link.toScene) != nextScene ||
                            !ScenePlanService::isLinkAllowed(link, routeMobilityReduced)) {
                            continue;
                        }
                        const std::vector<Vector2> candidatePath =
                            WalkablePathService::buildWalkablePath(mapData, playerPos, WalkablePathService::rectCenter(link.triggerRect));
                        if (candidatePath.empty()) continue;
                        const float len = WalkablePathService::polylineLength(candidatePath);
                        if (len < bestLen) {
                            bestLen = len;
                            bestPath = candidatePath;
                            bestLabel = link.label;
                        }
                    }

                    routePathPoints = std::move(bestPath);
                    routeNextHint = routePathPoints.empty()
                        ? "Could not trace local route"
                        : "Head to " + bestLabel + " to reach " +
                          sceneDisplayName(nextScene);

                    // Route progress: completed scene-legs + local approach on current leg.
                    const int totalLegs = std::max(1, static_cast<int>(routeScenePlan.size()) - 1);
                    int completedLegs = 0;
                    if (currentIt != routeScenePlan.end()) {
                        completedLegs = std::max(0, static_cast<int>(std::distance(routeScenePlan.begin(), currentIt)));
                    }

                    float localProgress = 0.0f;
                    if (!routePathPoints.empty()) {
                        const float currentToGoal = WalkablePathService::distanceBetween(playerPos, routePathPoints.back());
                        if (routeLegSceneId != currentSceneId || routeLegNextSceneId != nextScene ||
                            routeLegStartDistance <= 0.0f) {
                            routeLegSceneId = currentSceneId;
                            routeLegNextSceneId = nextScene;
                            routeLegStartDistance = std::max(currentToGoal, 1.0f);
                        }
                        localProgress = std::clamp(1.0f - (currentToGoal / routeLegStartDistance), 0.0f, 1.0f);
                    }

                    const float overallProgress =
                        ((static_cast<float>(completedLegs) + localProgress) / static_cast<float>(totalLegs)) * 100.0f;
                    routeProgressPct = std::max(routeProgressPct, std::clamp(overallProgress, 0.0f, 99.9f));
                }
            }
        }

        if (showNavigationGraph) {
            const std::vector<std::string> dfsOverlayNodes =
                (tabState.hasPath && tabState.lastAction == "PathDFS") ? tabState.lastPath.path
                                                                       : std::vector<std::string>{};
            dfsOverlayPathPoints = buildOverlayPathForScene(
                currentSceneName, dfsOverlayNodes, sceneLinks, mapData, playerPos,
                scenario_manager.isMobilityReduced(), sceneTargetPoint);

            const std::vector<std::string> alternateOverlayNodes =
                (tabState.hasPath &&
                 (tabState.lastAction == "AltPath" || tabState.lastAction == "BlockEdge" ||
                  tabState.lastAction == "BlockNode"))
                    ? tabState.lastPath.path
                    : std::vector<std::string>{};
            alternateOverlayPathPoints = buildOverlayPathForScene(
                currentSceneName, alternateOverlayNodes, sceneLinks, mapData, playerPos,
                scenario_manager.isMobilityReduced(), sceneTargetPoint);
        } else {
            dfsOverlayPathPoints.clear();
            alternateOverlayPathPoints.clear();
        }

        traversalRefreshCooldown -= dt;
        if (infoMenuOpen && traversalRefreshCooldown <= 0.0f) {
            const std::string traversalStart = canonicalSceneId(currentSceneName);
            dfsTraversalView = nav_service.runDfs(traversalStart, scenario_manager.isMobilityReduced());
            bfsTraversalView = nav_service.runBfs(traversalStart, scenario_manager.isMobilityReduced());
            traversalRefreshCooldown = 0.2f;
        }

        // --- Scene transition (portal & elevator detection + fade state machine) ---
        if (!infoMenuOpen) {
            transitions.update(WalkablePathService::playerColliderAt(playerPos), currentSceneName, dt);
        }

        // Perform scene swap at peak blackness (alpha == 1.0)
        if (transitions.needsSceneSwap()) {
            const TransitionRequest req = transitions.getPendingSwap();
            if (mapData.hasTexture) {
                UnloadTexture(mapData.texture);
                mapData.texture    = {};
                mapData.hasTexture = false;
            }
            mapData.hitboxes.clear();
            mapData.interestZones.clear();

            const auto scIt = sceneMap.find(req.targetScene);
            if (scIt != sceneMap.end()) {
                const std::string pngPath = AssetPathResolver::resolveAssetPath(argc > 0 ? argv[0] : nullptr, scIt->second.pngPath);
                if (!pngPath.empty()) {
                    mapData.texture    = LoadTexture(pngPath.c_str());
                    mapData.hasTexture = mapData.texture.id != 0;
                }
                const auto sdIt = sceneDataMap.find(req.targetScene);
                if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
                    mapData.hitboxes = sdIt->second.hitboxes;
                    mapData.interestZones = sdIt->second.interestZones;
                }
                currentSceneName = req.targetScene;
            }
            playerPos      = req.spawnPos;
            camera.target  = playerPos;
            camera.zoom    = 2.2f;
            clampCameraTarget(camera, mapData, screenWidth, screenHeight);
            transitions.notifySwapDone();
        }

        const bool isMoving = (moveX != 0.0f || moveY != 0.0f);
        if (isMoving) {
            playerAnim.timer += dt;
            const float frameStep = sprinting ? (1.0f / 16.0f) : (1.0f / 12.0f);
            if (playerAnim.timer >= frameStep) {
                playerAnim.timer = 0.0f;
                playerAnim.frame = (playerAnim.frame + 1) % SpriteAnimationService::directionalFrameCount(playerAnim.walkFrames);
            }
        } else {
            playerAnim.timer = 0.0f;
            playerAnim.frame = 0;
        }

        BeginDrawing();
        ClearBackground({18, 20, 28, 255});
        BeginMode2D(camera);
        if (mapData.hasTexture) {
            MapRenderService::drawMapWithHitboxes(mapData, showHitboxes);
        } else {
            DrawRectangle(0, 0, screenWidth, screenHeight, {22, 26, 36, 255});
        }

        if (showInterestZones && mapData.hasTexture) {
            MapRenderService::drawInterestZones(mapData.interestZones);
        }

        if (showNavigationGraph && mapData.hasTexture) {
            const std::vector<std::string> highlightedPathNodes =
                routeActive && !routeScenePlan.empty() ? routeScenePlan
                                                       : (tabState.hasPath ? tabState.lastPath.path
                                                                           : std::vector<std::string>{});
            drawCurrentSceneNavigationOverlay(
                graph,
                currentSceneName,
                sceneLinks,
                mapData.interestZones,
                highlightedPathNodes,
                resilience_service.getBlockedNodes(),
                scenario_manager.isMobilityReduced());

            if (routePathScene == currentSceneName && routePathPoints.size() >= 2) {
                const float pulse = 2.8f + std::sin(static_cast<float>(GetTime()) * 3.0f) * 0.8f;
                for (size_t i = 1; i < routePathPoints.size(); ++i) {
                    DrawLineEx(routePathPoints[i - 1], routePathPoints[i], pulse,
                               Color{255, 210, 70, 235});
                }
            }
            if (dfsOverlayPathPoints.size() >= 2) {
                for (size_t i = 1; i < dfsOverlayPathPoints.size(); ++i) {
                    DrawLineEx(dfsOverlayPathPoints[i - 1], dfsOverlayPathPoints[i], 3.0f,
                               Color{70, 255, 160, 220});
                }
            }
            if (alternateOverlayPathPoints.size() >= 2) {
                for (size_t i = 1; i < alternateOverlayPathPoints.size(); ++i) {
                    DrawLineEx(alternateOverlayPathPoints[i - 1], alternateOverlayPathPoints[i], 3.0f,
                               Color{255, 120, 120, 220});
                }
            }
        }

        // Debug: draw portal trigger zones as green semi-transparent rectangles
        if (showTriggers) {
            for (const auto& portal : transitions.getPortals()) {
                auto drawZone = [](const Rectangle& r) {
                    DrawRectangleRec(r, Color{0, 255, 0, 60});
                    DrawRectangleLinesEx(r, 1.5f, Color{0, 255, 0, 180});
                };
                if (portal.sceneA == currentSceneName) drawZone(portal.triggerA);
                if (portal.sceneB == currentSceneName) drawZone(portal.triggerB);
            }
            for (const auto& uniPortal : transitions.getUniPortals()) {
                if (uniPortal.scene == currentSceneName) {
                    DrawRectangleRec(uniPortal.triggerRect, Color{0, 255, 120, 60});
                    DrawRectangleLinesEx(uniPortal.triggerRect, 1.5f, Color{0, 255, 120, 200});
                }
            }
            for (const auto& elev : transitions.getElevators()) {
                if (elev.scene == currentSceneName) {
                    DrawRectangleRec(elev.triggerRect, Color{0, 180, 255, 60});
                    DrawRectangleLinesEx(elev.triggerRect, 1.5f, Color{0, 180, 255, 180});
                }
            }
        }

        const bool useWalk = isMoving && playerAnim.hasWalk;
        const bool canDrawSprite = playerAnim.hasIdle || playerAnim.hasWalk;
        if (canDrawSprite) {
            const Texture2D tex = useWalk ? playerAnim.walk : playerAnim.idle;
            const int activeFrames = useWalk ? playerAnim.walkFrames : playerAnim.idleFrames;
            const int baseFrame = SpriteAnimationService::directionStartFrame(playerAnim.direction, activeFrames);
            const int frameCount = SpriteAnimationService::directionalFrameCount(activeFrames);
            const int activeFrame = baseFrame + (playerAnim.frame % frameCount);
            const float frameW = static_cast<float>(playerAnim.frameWidth);
            const float frameH = static_cast<float>(playerAnim.frameHeight);
            Rectangle src{
                frameW * static_cast<float>(activeFrame),
                0.0f,
                frameW,
                frameH
            };
            Rectangle dst{
                playerPos.x,
                playerPos.y,
                frameW * playerRenderScale,
                frameH * playerRenderScale
            };
            DrawTexturePro(tex, src, dst, Vector2{dst.width * 0.5f, dst.height}, 0.0f, WHITE);
        } else {
            DrawCircleV(playerPos, 8.0f, RED);
        }
        EndMode2D();

        // -----------------------------------------------------------------------
        // Waze-style minimap (bottom-right corner, crops scene texture by radius)
        // -----------------------------------------------------------------------
        if (mapData.hasTexture && !infoMenuOpen) {
            constexpr int   kMapW        = 200;
            constexpr int   kMapH        = 150;
            constexpr int   kMapPad      = 12;
            constexpr float kWorldRadius = 300.0f;  // world-space crop radius

            const int mapX = screenWidth  - kMapW - kMapPad;
            const int mapY = screenHeight - kMapH - kMapPad;

            // Source rect: kWorldRadius around the player in scene space
            const float texW = static_cast<float>(mapData.texture.width);
            const float texH = static_cast<float>(mapData.texture.height);
            const float srcW = std::min(2.0f * kWorldRadius, texW);
            const float srcH = std::min(2.0f * kWorldRadius, texH);
            const float srcX = std::clamp(playerPos.x - srcW * 0.5f, 0.0f, std::max(0.0f, texW - srcW));
            const float srcY = std::clamp(playerPos.y - srcH * 0.5f, 0.0f, std::max(0.0f, texH - srcH));
            const Rectangle srcRect{srcX, srcY, srcW, srcH};

            auto worldToMini = [&](const Vector2& p) {
                return Vector2{
                    static_cast<float>(mapX) + ((p.x - srcRect.x) / srcRect.width) * kMapW,
                    static_cast<float>(mapY) + ((p.y - srcRect.y) / srcRect.height) * kMapH
                };
            };
            auto clampMiniPoint = [&](Vector2 p) {
                p.x = std::clamp(p.x, static_cast<float>(mapX), static_cast<float>(mapX + kMapW));
                p.y = std::clamp(p.y, static_cast<float>(mapY), static_cast<float>(mapY + kMapH));
                return p;
            };

            // Draw minimap background
            DrawRectangle(mapX - 2, mapY - 2, kMapW + 4, kMapH + 4, Color{0,0,0,200});
            // Draw scene texture cropped around player
            DrawTexturePro(mapData.texture,
                           srcRect,
                           Rectangle{static_cast<float>(mapX), static_cast<float>(mapY),
                                     static_cast<float>(kMapW), static_cast<float>(kMapH)},
                           Vector2{0, 0}, 0.0f, Color{255,255,255,210});

            for (const auto& hitbox : mapData.hitboxes) {
                const float left = std::max(hitbox.x, srcRect.x);
                const float top = std::max(hitbox.y, srcRect.y);
                const float right = std::min(hitbox.x + hitbox.width, srcRect.x + srcRect.width);
                const float bottom = std::min(hitbox.y + hitbox.height, srcRect.y + srcRect.height);
                if (right <= left || bottom <= top) continue;

                DrawRectangleRec(Rectangle{
                    static_cast<float>(mapX) + ((left - srcRect.x) / srcRect.width) * kMapW,
                    static_cast<float>(mapY) + ((top - srcRect.y) / srcRect.height) * kMapH,
                    ((right - left) / srcRect.width) * kMapW,
                    ((bottom - top) / srcRect.height) * kMapH
                }, Color{120, 120, 120, 135});
            }

            if (routeActive && routePathScene == currentSceneName && routePathPoints.size() >= 2) {
                for (size_t i = 1; i < routePathPoints.size(); ++i) {
                    const Vector2 a = clampMiniPoint(worldToMini(routePathPoints[i - 1]));
                    const Vector2 b = clampMiniPoint(worldToMini(routePathPoints[i]));
                    DrawLineEx(a, b, 3.0f, Color{255, 210, 60, 240});
                }
                const Vector2 goalMarker = clampMiniPoint(worldToMini(routePathPoints.back()));
                DrawCircleV(goalMarker, 5.0f, Color{255, 180, 60, 240});
                DrawCircleLines(static_cast<int>(goalMarker.x), static_cast<int>(goalMarker.y), 5.0f, BLACK);
            }

            const Vector2 playerMini = clampMiniPoint(worldToMini(playerPos));
            const float dotX = playerMini.x;
            const float dotY = playerMini.y;
            DrawCircle(static_cast<int>(dotX), static_cast<int>(dotY), 4.0f, Color{0,220,255,255});
            DrawCircleLines(static_cast<int>(dotX), static_cast<int>(dotY), 4.0f, WHITE);

            // Minimap border and label
            DrawRectangleLines(mapX - 2, mapY - 2, kMapW + 4, kMapH + 4, Color{80,160,255,200});
            DrawText("Map", mapX + 4, mapY + 4, 12, Color{180,220,255,220});
            if (routeActive) {
                DrawText("Route active", mapX + 52, mapY + 4, 12, Color{255,220,120,220});
            }
        }

        // --- Fade overlay (screen space) ---
        transitions.drawFadeOverlay(screenWidth, screenHeight);

        // --- "Presiona E" prompt ---
        if (transitions.isPromptVisible()) {
            const std::string hintText = transitions.getPromptHint();
            const char* hint        = hintText.c_str();
            const int   hintFontSz  = 22;
            const int   hintW       = MeasureText(hint, hintFontSz);
            const int   hintX       = (screenWidth  - hintW) / 2;
            const int   hintY       = screenHeight - 60;
            DrawRectangle(hintX - 10, hintY - 8, hintW + 20, hintFontSz + 16,
                          Color{0, 0, 0, 180});
            DrawText(hint, hintX, hintY, hintFontSz, YELLOW);
        }

        const char* coordText = TextFormat("Pos: (%.1f, %.1f)", playerPos.x, playerPos.y);
        const int coordFontSize = 20;
        const int coordPadding = 12;
        const int coordWidth = MeasureText(coordText, coordFontSize);
        const int coordX = screenWidth - coordWidth - coordPadding;
        const int coordY = coordPadding;
        DrawRectangle(coordX - 8, coordY - 6, coordWidth + 16, coordFontSize + 12, Color{0, 0, 0, 140});
        DrawText(coordText, coordX, coordY, coordFontSize, RAYWHITE);
        DrawText("M: Menu", 16, 12, 20, Color{220, 230, 255, 220});
        DrawText("TAB: POIs", 16, 34, 18, Color{255, 215, 120, 220});

        drawRaylibInfoMenu(
            infoMenuOpen,
            screenWidth,
            screenHeight,
            selectedRouteSceneIdx,
            routeActive,
            routeTargetScene,
            routeProgressPct,
            routeTravelElapsed,
            routeTravelCompleted,
            routeLegStartDistance,
            routeLegSceneId,
            routeLegNextSceneId,
            routeScenePlan,
            routePathPoints,
            routeNextHint,
            routeRefreshCooldown,
            routeScenes,
            sceneDisplayName,
            graph,
            dfsTraversalView,
            bfsTraversalView,
            rubricViewMode,
            graphViewPage,
            dfsViewPage,
            bfsViewPage,
            showNavigationGraph,
            tabState,
            currentSceneName,
            showHitboxes,
            showTriggers,
            showInterestZones,
            scenario_manager.isMobilityReduced(),
            scenario_manager.getStudentType(),
            resilience_service.getBlockedNodes(),
            resilience_service.isStillConnected());

        rlImGuiBegin();

        renderAcademicRuntimeOverlay(
            showNavigationGraph,
            showInterestZones,
            tabState,
            nav_service,
            scenario_manager,
            complexity_analyzer,
            resilience_service,
            graph,
            sceneDataMap,
            routeScenes,
            sceneDisplayName,
            currentSceneName,
            routeActive,
            routeScenePlan);

        // Floor-elevator menu (shown when player presses E near an elevator)
        if (!infoMenuOpen) {
            transitions.drawFloorMenu();
        }
        rlImGuiEnd();

        EndDrawing();

    }

    if (mapData.hasTexture) {
        UnloadTexture(mapData.texture);
    }
    if (playerAnim.hasIdle) UnloadTexture(playerAnim.idle);
    if (playerAnim.hasWalk) UnloadTexture(playerAnim.walk);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}


