#include "TabManager.h"

#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static PathResult mergeSegmentedPaths(const std::vector<PathResult>& segments) {
    PathResult out;
    if (segments.empty()) return out;

    out.found = true;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (!segments[i].found || segments[i].path.empty()) {
            out = {};
            return out;
        }
        out.total_weight += segments[i].total_weight;
        if (i == 0) out.path = segments[i].path;
        else out.path.insert(out.path.end(), segments[i].path.begin() + 1, segments[i].path.end());
    }
    return out;
}

static PathResult runProfiledPathDfs(const CampusGraph& graph,
                                     NavigationService& navService,
                                     ScenarioManager& scenarioManager,
                                     const std::string& origin,
                                     const std::string& destination) {
    const auto waypoints = scenarioManager.applyProfile(graph, origin, destination);
    if (waypoints.size() < 2) return {};

    std::vector<PathResult> segments;
    segments.reserve(waypoints.size() - 1);
    for (size_t i = 1; i < waypoints.size(); ++i) {
        segments.push_back(navService.findPathDfs(waypoints[i - 1], waypoints[i], scenarioManager.isMobilityReduced()));
    }
    return mergeSegmentedPaths(segments);
}

static PathResult runProfiledPathDijkstra(const CampusGraph& graph,
                                          NavigationService& navService,
                                          ScenarioManager& scenarioManager,
                                          const std::string& origin,
                                          const std::string& destination) {
    const auto waypoints = scenarioManager.applyProfile(graph, origin, destination);
    if (waypoints.size() < 2) return {};

    std::vector<PathResult> segments;
    segments.reserve(waypoints.size() - 1);
    for (size_t i = 1; i < waypoints.size(); ++i) {
        segments.push_back(navService.findPath(waypoints[i - 1], waypoints[i], scenarioManager.isMobilityReduced()));
    }
    return mergeSegmentedPaths(segments);
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
        segments.push_back(resilienceService.findAlternatePath(
            waypoints[i - 1], waypoints[i], scenarioManager.isMobilityReduced()));
    }
    return mergeSegmentedPaths(segments);
}

static CampusValidationReport validateCampusJson(const std::string& campusJsonPath) {
    CampusValidationReport report;
    std::ifstream input(campusJsonPath);
    if (!input.is_open()) {
        report.issues.push_back("No se pudo abrir campus.json para validacion.");
        return report;
    }

    json data;
    try {
        input >> data;
    } catch (...) {
        report.issues.push_back("campus.json tiene formato JSON invalido.");
        return report;
    }

    if (!data.contains("nodes") || !data["nodes"].is_array() ||
        !data.contains("edges") || !data["edges"].is_array()) {
        report.issues.push_back("campus.json debe contener arreglos 'nodes' y 'edges'.");
        return report;
    }

    report.nodeCount = static_cast<int>(data["nodes"].size());
    report.edgeCount = static_cast<int>(data["edges"].size());

    for (const auto& n : data["nodes"]) {
        const std::string name = toLower(n.value("name", ""));
        if (name.find("biblio") != std::string::npos || name.find("biblioteca") != std::string::npos) {
            report.hasBibliotecaNode = true;
        }
        if (name.find("soda") != std::string::npos || name.find("comedor") != std::string::npos ||
            name.find("cafeter") != std::string::npos) {
            report.hasSodaOrComedorNode = true;
        }
    }

    for (const auto& e : data["edges"]) {
        if (!e.contains("mobility_weight") || !e.contains("blocked_for_mr")) {
            report.edgesMissingMobilityFields++;
        }
        const double baseWeight = e.value("base_weight", 0.0);
        if (baseWeight <= 0.0) {
            report.edgesWithNonPositiveWeight++;
        }
    }

    if (report.nodeCount < 8) report.issues.push_back("Se requieren al menos 8 nodos.");
    if (report.edgeCount < 10) report.issues.push_back("Se requieren al menos 10 aristas.");
    if (report.edgesMissingMobilityFields > 0) {
        report.issues.push_back("Hay aristas sin mobility_weight o blocked_for_mr.");
    }
    if (!report.hasBibliotecaNode) report.issues.push_back("No se detecto nodo de Biblioteca.");
    if (!report.hasSodaOrComedorNode) report.issues.push_back("No se detecto nodo de Soda/Comedor.");
    if (report.edgesWithNonPositiveWeight > 0) {
        report.issues.push_back("Hay aristas con peso base no positivo.");
    }

    return report;
}
} // namespace

TabManagerState createTabManagerState(const CampusGraph& graph, const std::string& campusJsonPath) {
    TabManagerState state;
    const auto ids = graph.nodeIds();
    const std::string start = ids.empty() ? "" : ids.front();
    const std::string end = ids.size() > 1 ? ids[1] : start;
    std::snprintf(state.startId, sizeof(state.startId), "%s", start.c_str());
    std::snprintf(state.endId, sizeof(state.endId), "%s", end.c_str());
    state.validation = validateCampusJson(campusJsonPath);
    return state;
}

void renderMinimapRouteWindow(
    int screenWidth,
    int screenHeight,
    int& selectedRouteSceneIdx,
    bool& routeActive,
    std::string& routeTargetScene,
    std::vector<std::string>& routeScenePlan,
    std::vector<Vector2>& routePathPoints,
    std::string& routeNextHint,
    float& routeRefreshCooldown,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName) {
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(screenWidth - 286),
                                   static_cast<float>(screenHeight - 334)),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(270, 150), ImGuiCond_Always);
    ImGui::Begin("Ruta minimapa", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const char* selectedRouteLabel = routeScenes[selectedRouteSceneIdx].second.c_str();
    if (ImGui::BeginCombo("Destino", selectedRouteLabel)) {
        for (int i = 0; i < static_cast<int>(routeScenes.size()); ++i) {
            const bool isSelected = selectedRouteSceneIdx == i;
            if (ImGui::Selectable(routeScenes[i].second.c_str(), isSelected)) selectedRouteSceneIdx = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Trazar ruta", ImVec2(122, 0))) {
        routeActive = true;
        routeTargetScene = routeScenes[selectedRouteSceneIdx].first;
        routeRefreshCooldown = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Limpiar", ImVec2(122, 0))) {
        routeActive = false;
        routeTargetScene.clear();
        routeScenePlan.clear();
        routePathPoints.clear();
        routeNextHint.clear();
    }

    if (routeActive) {
        ImGui::Separator();
        ImGui::TextWrapped("Destino: %s", sceneDisplayName(routeTargetScene).c_str());
        ImGui::TextWrapped("%s", routeNextHint.c_str());
    }

    ImGui::End();
}

void renderAcademicControlPanel(
    TabManagerState& state,
    NavigationService& navService,
    ScenarioManager& scenarioManager,
    ComplexityAnalyzer& complexityAnalyzer,
    ResilienceService& resilienceService,
    const CampusGraph& graph,
    const std::string& currentSceneName,
    bool& showHitboxes,
    bool& showTriggers) {
    ImGui::SetNextWindowSize(ImVec2(430, 510), ImGuiCond_FirstUseEver);
    ImGui::Begin("Control Academico");

    ImGui::Text("Escena actual: %s", currentSceneName.c_str());
    ImGui::Checkbox("Hitboxes", &showHitboxes);
    ImGui::SameLine();
    ImGui::Checkbox("Triggers", &showTriggers);
    ImGui::Separator();

    bool mobilityReduced = scenarioManager.isMobilityReduced();
    if (ImGui::Checkbox("Movilidad reducida", &mobilityReduced)) {
        scenarioManager.setMobilityReduced(mobilityReduced);
    }

    int studentType = scenarioManager.getStudentType() == StudentType::NEW_STUDENT ? 0 : 1;
    if (ImGui::RadioButton("Estudiante nuevo", studentType == 0)) {
        scenarioManager.setStudentType(StudentType::NEW_STUDENT);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Estudiante regular", studentType == 1)) {
        scenarioManager.setStudentType(StudentType::REGULAR_STUDENT);
    }

    ImGui::InputText("Origen", state.startId, sizeof(state.startId));
    ImGui::InputText("Destino", state.endId, sizeof(state.endId));
    ImGui::InputText("Nodo (resiliencia)", state.nodeId, sizeof(state.nodeId));
    if (ImGui::BeginTabBar("RubricaTabs")) {
        if (ImGui::BeginTabItem("1.DFS")) {
            if (ImGui::Button("Ejecutar DFS")) {
                state.lastTraversal = navService.runDfs(state.startId, scenarioManager.isMobilityReduced());
                state.hasTraversal = true;
                state.lastAction = "DFS";
            }
            if (state.lastAction == "DFS" && state.hasTraversal) {
                ImGui::Text("Nodos visitados: %d", state.lastTraversal.nodes_visited);
                ImGui::Text("Tiempo: %lld us", state.lastTraversal.elapsed_us);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("2.BFS")) {
            if (ImGui::Button("Ejecutar BFS")) {
                state.lastTraversal = navService.runBfs(state.startId, scenarioManager.isMobilityReduced());
                state.hasTraversal = true;
                state.lastAction = "BFS";
            }
            if (state.lastAction == "BFS" && state.hasTraversal) {
                ImGui::Text("Nodos visitados: %d", state.lastTraversal.nodes_visited);
                ImGui::Text("Tiempo: %lld us", state.lastTraversal.elapsed_us);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("3.Conectividad")) {
            if (ImGui::Button("Evaluar conectividad")) {
                state.lastConnected = navService.checkConnectivity();
                state.lastAction = "Connectivity";
            }
            ImGui::Text("Estado: %s", state.lastConnected ? "Conexo" : "No conexo");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("4.Camino Optimo")) {
            if (ImGui::Button("Dijkstra perfilado")) {
                state.lastPath = runProfiledPathDijkstra(graph, navService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "PathDijkstra";
            }
            if (state.lastAction == "PathDijkstra" && state.hasPath) {
                ImGui::Text("Encontrado: %s", state.lastPath.found ? "si" : "no");
                ImGui::Text("Peso total: %.2f", state.lastPath.total_weight);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("5.Camino DFS")) {
            if (ImGui::Button("Ejecutar DFS (rubrica)")) {
                state.lastPath = runProfiledPathDfs(graph, navService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "PathDFS";
            }
            if (state.lastAction == "PathDFS" && state.hasPath) {
                ImGui::Text("Encontrado: %s", state.lastPath.found ? "si" : "no");
                ImGui::Text("Peso total: %.2f", state.lastPath.total_weight);
                ImGui::Text("Nodos del camino:");
                for (const auto& id : state.lastPath.path) ImGui::BulletText("%s", id.c_str());
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("6.Escenarios")) {
            const auto prof = scenarioManager.applyProfile(graph, state.startId, state.endId);
            ImGui::Text("Perfil aplicado:");
            for (const auto& step : prof) ImGui::BulletText("%s", step.c_str());
            ImGui::TextWrapped("Si es estudiante nuevo, se fuerza paso intermedio por Biblioteca y Soda/Comedor cuando existen nodos compatibles.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("7.Complejidad")) {
            if (ImGui::Button("Comparar BFS vs DFS")) {
                state.lastStats = complexityAnalyzer.analyze(state.startId, scenarioManager.isMobilityReduced());
                state.lastComparison = complexityAnalyzer.compareAlgorithms(
                    state.startId, state.endId, scenarioManager.isMobilityReduced());
                state.hasComparison = true;
                state.lastAction = "Complexity";
            }
            if (state.hasComparison) {
                for (const auto& stat : state.lastStats) {
                    ImGui::Text("%s: %d nodos, %lld us", stat.algorithm.c_str(), stat.nodes_visited, stat.elapsed_us);
                }
                ImGui::Separator();
                ImGui::Text("DFS->destino: %s", state.lastComparison.dfs_reaches_destination ? "si" : "no");
                ImGui::Text("BFS->destino: %s", state.lastComparison.bfs_reaches_destination ? "si" : "no");
                ImGui::Text("Delta nodos (BFS-DFS): %d", state.lastComparison.delta_nodes_visited);
                ImGui::Text("Delta tiempo us (BFS-DFS): %lld", state.lastComparison.delta_elapsed_us);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("8.Resiliencia")) {
            if (ImGui::Button("Ruta alterna")) {
                state.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "AltPath";
            }
            ImGui::SameLine();
            if (ImGui::Button("Bloquear arista")) {
                resilienceService.blockEdge(state.startId, state.endId);
                state.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "BlockEdge";
            }
            ImGui::SameLine();
            if (ImGui::Button("Bloquear nodo")) {
                resilienceService.blockNode(state.nodeId);
                state.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "BlockNode";
            }
            if (ImGui::Button("Desbloquear todo")) {
                resilienceService.unblockAll();
                state.lastAction = "UnblockAll";
            }

            const auto blockedNodes = resilienceService.getBlockedNodes();
            if (!blockedNodes.empty()) {
                ImGui::Text("Nodos bloqueados:");
                for (const auto& id : blockedNodes) ImGui::BulletText("%s", id.c_str());
            }
            if (state.hasPath) {
                ImGui::Text("Ruta alterna encontrada: %s", state.lastPath.found ? "si" : "no");
                ImGui::Text("Peso: %.2f", state.lastPath.total_weight);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Text("Validacion campus.json");
    ImGui::Text("Nodos=%d Aristas=%d", state.validation.nodeCount, state.validation.edgeCount);
    if (state.validation.issues.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Validacion minima OK");
    } else {
        for (const auto& issue : state.validation.issues) {
            ImGui::BulletText("%s", issue.c_str());
        }
    }

    ImGui::End();
}
