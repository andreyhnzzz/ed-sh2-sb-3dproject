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
    (void)navService;
    return scenarioManager.buildProfiledPath(graph, origin, destination);
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
        report.issues.push_back("Could not open campus.json for validation.");
        return report;
    }

    json data;
    try {
        input >> data;
    } catch (...) {
        report.issues.push_back("campus.json has invalid JSON format.");
        return report;
    }

    if (!data.contains("nodes") || !data["nodes"].is_array() ||
        !data.contains("edges") || !data["edges"].is_array()) {
        report.issues.push_back("campus.json must contain 'nodes' and 'edges' arrays.");
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

    if (report.nodeCount < 8) report.issues.push_back("At least 8 nodes are required.");
    if (report.edgeCount < 10) report.issues.push_back("At least 10 edges are required.");
    if (report.edgesMissingMobilityFields > 0) {
        report.issues.push_back("Some edges are missing mobility_weight or blocked_for_mr.");
    }
    if (!report.hasBibliotecaNode) report.issues.push_back("No Library node detected.");
    if (!report.hasSodaOrComedorNode) report.issues.push_back("No Cafeteria/Dining node detected.");
    if (report.edgesWithNonPositiveWeight > 0) {
        report.issues.push_back("Some edges have non-positive base weight.");
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

static void renderRouteControlsContent(
    int& selectedRouteSceneIdx,
    bool& routeActive,
    std::string& routeTargetScene,
    std::vector<std::string>& routeScenePlan,
    std::vector<Vector2>& routePathPoints,
    std::string& routeNextHint,
    float& routeRefreshCooldown,
    const std::vector<std::pair<std::string, std::string>>& routeScenes,
    const std::function<std::string(const std::string&)>& sceneDisplayName) {
    const char* selectedRouteLabel = routeScenes[selectedRouteSceneIdx].second.c_str();
    if (ImGui::BeginCombo("Destination", selectedRouteLabel)) {
        for (int i = 0; i < static_cast<int>(routeScenes.size()); ++i) {
            const bool isSelected = selectedRouteSceneIdx == i;
            if (ImGui::Selectable(routeScenes[i].second.c_str(), isSelected)) selectedRouteSceneIdx = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Draw Route", ImVec2(122, 0))) {
        routeActive = true;
        routeTargetScene = routeScenes[selectedRouteSceneIdx].first;
        routeRefreshCooldown = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(122, 0))) {
        routeActive = false;
        routeTargetScene.clear();
        routeScenePlan.clear();
        routePathPoints.clear();
        routeNextHint.clear();
    }

    if (routeActive) {
        ImGui::Separator();
        ImGui::TextWrapped("Destination: %s", sceneDisplayName(routeTargetScene).c_str());
        ImGui::TextWrapped("%s", routeNextHint.c_str());
        if (!routeScenePlan.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted("Scene plan:");
            for (const auto& sceneId : routeScenePlan) {
                ImGui::BulletText("%s", sceneDisplayName(sceneId).c_str());
            }
        }
    }
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
    ImGui::Begin("Minimap Route", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    renderRouteControlsContent(selectedRouteSceneIdx, routeActive, routeTargetScene,
                               routeScenePlan, routePathPoints, routeNextHint,
                               routeRefreshCooldown, routeScenes, sceneDisplayName);
    ImGui::End();
}

static void renderAcademicControlPanelContent(
    TabManagerState& state,
    NavigationService& navService,
    ScenarioManager& scenarioManager,
    ComplexityAnalyzer& complexityAnalyzer,
    ResilienceService& resilienceService,
    const CampusGraph& graph,
    const std::string& currentSceneName,
    bool& showHitboxes,
    bool& showTriggers,
    bool& showInterestZones) {
    ImGui::Text("Current scene: %s", currentSceneName.c_str());
    ImGui::Checkbox("Hitboxes", &showHitboxes);
    ImGui::SameLine();
    ImGui::Checkbox("Triggers", &showTriggers);
    ImGui::SameLine();
    ImGui::Checkbox("Interes", &showInterestZones);
    ImGui::Separator();

    bool mobilityReduced = scenarioManager.isMobilityReduced();
    if (ImGui::Checkbox("Reduced mobility", &mobilityReduced)) {
        scenarioManager.setMobilityReduced(mobilityReduced);
    }

    int studentType = scenarioManager.getStudentType() == StudentType::NEW_STUDENT ? 0 : 1;
    if (ImGui::RadioButton("New student", studentType == 0)) {
        scenarioManager.setStudentType(StudentType::NEW_STUDENT);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Regular student", studentType == 1)) {
        scenarioManager.setStudentType(StudentType::REGULAR_STUDENT);
    }

    ImGui::InputText("Origin", state.startId, sizeof(state.startId));
    ImGui::InputText("Destination", state.endId, sizeof(state.endId));
    ImGui::InputText("Node (resilience)", state.nodeId, sizeof(state.nodeId));
    if (ImGui::BeginTabBar("RubricaTabs")) {
        if (ImGui::BeginTabItem("1.DFS")) {
            if (ImGui::Button("Run DFS")) {
                state.lastTraversal = navService.runDfs(state.startId, scenarioManager.isMobilityReduced());
                state.hasTraversal = true;
                state.lastAction = "DFS";
            }
            if (state.lastAction == "DFS" && state.hasTraversal) {
                ImGui::Text("Visited nodes: %d", state.lastTraversal.nodes_visited);
                ImGui::Text("Time: %lld us", state.lastTraversal.elapsed_us);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("2.BFS")) {
            if (ImGui::Button("Run BFS")) {
                state.lastTraversal = navService.runBfs(state.startId, scenarioManager.isMobilityReduced());
                state.hasTraversal = true;
                state.lastAction = "BFS";
            }
            if (state.lastAction == "BFS" && state.hasTraversal) {
                ImGui::Text("Visited nodes: %d", state.lastTraversal.nodes_visited);
                ImGui::Text("Time: %lld us", state.lastTraversal.elapsed_us);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("3.Connectivity")) {
            if (ImGui::Button("Check connectivity")) {
                state.lastConnected = navService.checkConnectivity();
                state.lastAction = "Connectivity";
            }
            ImGui::Text("Status: %s", state.lastConnected ? "Connected" : "Not connected");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("4.Shortest Path")) {
            if (ImGui::Button("Profiled Dijkstra")) {
                state.lastPath = runProfiledPathDijkstra(graph, navService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "PathDijkstra";
            }
            if (state.lastAction == "PathDijkstra" && state.hasPath) {
                ImGui::Text("Found: %s", state.lastPath.found ? "yes" : "no");
                ImGui::Text("Total weight: %.2f", state.lastPath.total_weight);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("5.DFS Path")) {
            if (ImGui::Button("Run DFS (rubric)")) {
                state.lastPath = runProfiledPathDfs(graph, navService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "PathDFS";
            }
            if (state.lastAction == "PathDFS" && state.hasPath) {
                ImGui::Text("Found: %s", state.lastPath.found ? "yes" : "no");
                ImGui::Text("Total weight: %.2f", state.lastPath.total_weight);
                ImGui::Text("Path nodes:");
                for (const auto& id : state.lastPath.path) ImGui::BulletText("%s", id.c_str());
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("6.Scenarios")) {
            const auto prof = scenarioManager.applyProfile(graph, state.startId, state.endId);
            ImGui::Text("Applied profile:");
            for (const auto& step : prof) ImGui::BulletText("%s", step.c_str());
            ImGui::TextWrapped("If student type is New, route is forced through Library and Cafeteria/Dining when compatible nodes exist.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("7.Complexity")) {
            if (ImGui::Button("Compare BFS vs DFS")) {
                state.lastStats = complexityAnalyzer.analyze(state.startId, scenarioManager.isMobilityReduced());
                state.lastComparison = complexityAnalyzer.compareAlgorithms(
                    state.startId, state.endId, scenarioManager.isMobilityReduced());
                state.hasComparison = true;
                state.lastAction = "Complexity";
            }
            if (state.hasComparison) {
                for (const auto& stat : state.lastStats) {
                    ImGui::Text("%s: %d nodes, %lld us", stat.algorithm.c_str(), stat.nodes_visited, stat.elapsed_us);
                }
                ImGui::Separator();
                ImGui::Text("DFS->destination: %s", state.lastComparison.dfs_reaches_destination ? "yes" : "no");
                ImGui::Text("BFS->destination: %s", state.lastComparison.bfs_reaches_destination ? "yes" : "no");
                ImGui::Text("Node delta (BFS-DFS): %d", state.lastComparison.delta_nodes_visited);
                ImGui::Text("Time delta us (BFS-DFS): %lld", state.lastComparison.delta_elapsed_us);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("8.Resilience")) {
            if (ImGui::Button("Alternate route")) {
                state.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "AltPath";
            }
            ImGui::SameLine();
            if (ImGui::Button("Block edge")) {
                resilienceService.blockEdge(state.startId, state.endId);
                state.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "BlockEdge";
            }
            ImGui::SameLine();
            if (ImGui::Button("Block node")) {
                resilienceService.blockNode(state.nodeId);
                state.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager, state.startId, state.endId);
                state.hasPath = true;
                state.lastAction = "BlockNode";
            }
            if (ImGui::Button("Unblock all")) {
                resilienceService.unblockAll();
                state.lastAction = "UnblockAll";
            }

            const auto blockedNodes = resilienceService.getBlockedNodes();
            if (!blockedNodes.empty()) {
                ImGui::Text("Blocked nodes:");
                for (const auto& id : blockedNodes) ImGui::BulletText("%s", id.c_str());
            }
            if (state.hasPath) {
                ImGui::Text("Alternate route found: %s", state.lastPath.found ? "yes" : "no");
                ImGui::Text("Weight: %.2f", state.lastPath.total_weight);
            }
            ImGui::Text("Global connectivity: %s", resilienceService.isStillConnected() ? "connected" : "fragmented");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Text("campus.json validation");
    ImGui::Text("Nodes=%d Edges=%d", state.validation.nodeCount, state.validation.edgeCount);
    if (state.validation.issues.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Minimum validation OK");
    } else {
        for (const auto& issue : state.validation.issues) {
            ImGui::BulletText("%s", issue.c_str());
        }
    }
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
    bool& showTriggers,
    bool& showInterestZones) {
    ImGui::SetNextWindowSize(ImVec2(430, 510), ImGuiCond_FirstUseEver);
    ImGui::Begin("Academic Control");

    renderAcademicControlPanelContent(state, navService, scenarioManager, complexityAnalyzer,
                                      resilienceService, graph, currentSceneName,
                                      showHitboxes, showTriggers, showInterestZones);

    ImGui::End();
}

void renderFullScreenInfoMenu(
    bool& isOpen,
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
    const std::function<std::string(const std::string&)>& sceneDisplayName,
    TabManagerState& state,
    NavigationService& navService,
    ScenarioManager& scenarioManager,
    ComplexityAnalyzer& complexityAnalyzer,
    ResilienceService& resilienceService,
    const CampusGraph& graph,
    const std::string& currentSceneName,
    bool& showHitboxes,
    bool& showTriggers,
    bool& showInterestZones) {
    if (!isOpen) return;

    // 1) Nearly-solid panel backgrounds
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.12f, 0.96f));

    // 2) Remove global dim behind menu (transparent root window)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    // 3) Max text contrast
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.85f, 0.87f, 0.90f, 1.0f));

    // 5) Lower blue saturation for controls
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.24f, 0.36f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.30f, 0.43f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.33f, 0.48f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.28f, 0.43f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.34f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.38f, 0.55f, 1.0f));

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenWidth),
                                    static_cast<float>(screenHeight)),
                             ImGuiCond_Always);
    ImGui::Begin("Information Menu",
                 &isOpen,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBackground);

    ImGui::Text("Information menu");
    ImGui::SameLine();
    ImGui::TextDisabled("(M to close)");
    ImGui::Text("Current scene: %s", currentSceneName.c_str());
    ImGui::TextUnformatted("UTF-8 test: aeioun Nu - us");
    ImGui::Separator();

    const ImVec2 region = ImGui::GetContentRegionAvail();
    const float leftWidth = std::max(320.0f, region.x * 0.33f);
    const float rightWidth = std::max(320.0f, region.x - leftWidth - 10.0f);
    const float panelHeight = std::max(200.0f, region.y);

    ImGui::BeginChild("RoutePanel", ImVec2(leftWidth, panelHeight), true, ImGuiWindowFlags_None);
    ImGui::TextUnformatted("Route and navigation");
    ImGui::Separator();
    renderRouteControlsContent(selectedRouteSceneIdx, routeActive, routeTargetScene,
                               routeScenePlan, routePathPoints, routeNextHint,
                               routeRefreshCooldown, routeScenes, sceneDisplayName);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("AcademicPanel", ImVec2(rightWidth, panelHeight), true,
                      ImGuiWindowFlags_None);
    ImGui::TextUnformatted("Academic control");
    ImGui::Separator();
    renderAcademicControlPanelContent(state, navService, scenarioManager, complexityAnalyzer,
                                      resilienceService, graph, currentSceneName,
                                      showHitboxes, showTriggers, showInterestZones);
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor(10);
    ImGui::PopStyleVar();
}

