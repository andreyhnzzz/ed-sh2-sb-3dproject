#include "UIManager.h"

#include "rlImGui.h"
#include "imgui.h"

#include "services/RuntimeTextService.h"
#include "services/ScenePlanService.h"
#include "services/StringUtils.h"
#include "services/WalkablePathService.h"
#include "services/AudioManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace {
constexpr double kPixelsToMeters = 0.10;

struct VisualPoiNode {
    std::string sceneId;
    std::string label;
    Vector2 worldPos{0.0f, 0.0f};
};

bool isOverlayEdgeAllowed(const Edge& edge, bool mobilityReduced) {
    if (edge.currently_blocked) return false;
    if (mobilityReduced && edge.blocked_for_mr) return false;
    return true;
}

bool pathContainsDirectedStep(const std::vector<std::string>& path,
                              const std::string& from,
                              const std::string& to) {
    for (size_t i = 1; i < path.size(); ++i) {
        if (path[i - 1] == from && path[i] == to) return true;
    }
    return false;
}

std::vector<VisualPoiNode> collectVisualPoiNodes(
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

int countProfileDiscardedEdges(const CampusGraph& graph, bool mobilityReduced) {
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

std::string buildSelectionCriterion(StudentType studentType, bool mobilityReduced) {
    if (studentType == StudentType::DISABLED_STUDENT || mobilityReduced) {
        return "Bloquea todas las escaleras";
    }
    if (studentType == StudentType::NEW_STUDENT) {
        return "Pasa obligatoriamente por al menos un POI";
    }
    return "Ruta mas corta";
}

Vector2 routeLeadAnchor(const Vector2& playerPos, const Vector2& nextPoint) {
    const Vector2 delta{nextPoint.x - playerPos.x, nextPoint.y - playerPos.y};
    const float len = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (len <= 0.001f) return playerPos;
    constexpr float kLead = 12.0f;
    return Vector2{
        playerPos.x + (delta.x / len) * kLead,
        playerPos.y + (delta.y / len) * kLead
    };
}

PathResult mergeProfiledSegments(const std::vector<PathResult>& segments) {
    PathResult merged;
    if (segments.empty()) return merged;

    merged.found = true;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        if (!segment.found || segment.path.empty()) return {};

        merged.total_weight += segment.total_weight;
        if (i == 0) {
            merged.path = segment.path;
        } else {
            merged.path.insert(merged.path.end(), segment.path.begin() + 1, segment.path.end());
        }
    }
    return merged;
}

PathResult runProfiledDfsPath(const CampusGraph& graph,
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

PathResult runProfiledAlternatePath(const CampusGraph& graph,
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

bool comboSelectNode(const char* label,
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

bool drawHorizontalSlider(const Rectangle& track,
                          float& value,
                          float minValue,
                          float maxValue,
                          const Color& trackColor,
                          const Color& fillColor,
                          const Color& knobColor) {
    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse, track);
    const bool dragging = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    if (dragging && maxValue > minValue) {
        const float ratio = std::clamp((mouse.x - track.x) / track.width, 0.0f, 1.0f);
        value = minValue + (maxValue - minValue) * ratio;
    }

    DrawRectangleRounded(track, 0.45f, 8, trackColor);
    const float ratio = (maxValue > minValue) ? std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f)
                                              : 0.0f;
    Rectangle fill = track;
    fill.width *= ratio;
    DrawRectangleRounded(fill, 0.45f, 8, fillColor);
    DrawCircleV(Vector2{track.x + ratio * track.width, track.y + track.height * 0.5f},
                track.height * 0.42f, knobColor);
    return dragging;
}

bool linkMatchesEdgeType(SceneLinkType linkType, const std::string& edgeType) {
    const std::string lowered = StringUtils::toLowerCopy(edgeType);
    switch (linkType) {
        case SceneLinkType::Elevator: return lowered.find("elev") != std::string::npos;
        case SceneLinkType::StairLeft:
        case SceneLinkType::StairRight:
            return lowered.find("escal") != std::string::npos ||
                   lowered.find("stair") != std::string::npos;
        case SceneLinkType::Portal:
        default: return lowered.find("portal") != std::string::npos;
    }
}

const Edge* findBestEdgeForLink(const CampusGraph& graph,
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

bool drawRayButton(const Rectangle& r,
                   const char* label,
                   int fontSize,
                   Color base,
                   Color hover,
                   Color active,
                   Color textColor) {
    const Vector2 mouse = GetMousePosition();
    const bool inside = CheckCollisionPointRec(mouse, r);
    const bool pressed = inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const Color c = pressed ? active : (inside ? hover : base);
    DrawRectangleRec(r, c);
    const int tw = MeasureText(label, fontSize);
    DrawText(label,
             static_cast<int>(r.x + (r.width - tw) * 0.5f),
             static_cast<int>(r.y + (r.height - fontSize) * 0.5f),
             fontSize,
             textColor);
    return pressed;
}

const char* studentTypeToLabel(StudentType studentType) {
    switch (studentType) {
        case StudentType::NEW_STUDENT: return "New";
        case StudentType::DISABLED_STUDENT: return "Disabled";
        case StudentType::VETERAN_STUDENT:
        default: return "Veteran";
    }
}

Color lightenColor(Color c, int amount) {
    auto up = [amount](unsigned char v) -> unsigned char {
        const int boosted = static_cast<int>(v) + amount;
        return static_cast<unsigned char>(std::clamp(boosted, 0, 255));
    };
    return Color{up(c.r), up(c.g), up(c.b), c.a};
}

Color nodeLevelColor(const std::string& nodeIdLower) {
    // Nivel base para escenas generales del campus.
    if (nodeIdLower == "piso1") return Color{90, 180, 255, 235};
    if (nodeIdLower == "piso2") return Color{120, 210, 255, 235};
    // Biblioteca comparte color con piso 3.
    if (nodeIdLower == "piso3" || nodeIdLower == "biblio") return Color{140, 230, 170, 235};
    if (nodeIdLower == "piso4") return Color{255, 200, 120, 235};
    if (nodeIdLower == "piso5") return Color{255, 150, 150, 235};
    return Color{170, 190, 230, 235};
}

void drawCurrentSceneNavigationOverlay(const CampusGraph& graph,
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
    const bool isCurrentScene = StringUtils::toLowerCopy(currentSceneName) == currentSceneId;

    std::unordered_set<std::string> drawnTargets;
    for (const auto& link : sceneLinks) {
        if (StringUtils::toLowerCopy(link.fromScene) != currentSceneId) continue;

        const Edge* edge = findBestEdgeForLink(graph, currentSceneId, link);
        const std::string dedupeKey = currentSceneId + "|" + StringUtils::toLowerCopy(link.toScene) + "|" +
                                      (edge ? edge->type : std::string("link"));
        if (!drawnTargets.insert(dedupeKey).second) continue;

        const Vector2 targetPos = WalkablePathService::rectCenter(link.triggerRect);
        const bool edgeAllowed = edge ? isOverlayEdgeAllowed(*edge, mobilityReduced)
                                      : ScenePlanService::isLinkAllowed(link, mobilityReduced);
        const bool onActivePath = pathContainsDirectedStep(activePathNodes, currentSceneId,
                                                           StringUtils::toLowerCopy(link.toScene));

        const Color edgeColor = onActivePath
            ? Color{170, 95, 255, 255}
            : (edgeAllowed ? Color{170, 205, 255, 200} : Color{220, 90, 90, 180});
        DrawLineEx(scenePos, targetPos, onActivePath ? 4.0f : 2.0f, edgeColor);
        DrawCircleV(targetPos, 6.0f, edgeColor);
        DrawCircleLines(static_cast<int>(targetPos.x), static_cast<int>(targetPos.y), 6.0f, BLACK);

        if (edge) {
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
        const bool onActivePath = pathContainsDirectedStep(activePathNodes, currentSceneId, poiEdge.toNodeId);
        bool edgeAllowed = true;
        for (const auto& edge : graph.edgesFrom(currentSceneId)) {
            if (edge.to == poiEdge.toNodeId) {
                edgeAllowed = isOverlayEdgeAllowed(edge, mobilityReduced);
                break;
            }
        }

        const Color edgeColor = onActivePath
            ? Color{180, 110, 255, 255}
            : (edgeAllowed ? Color{170, 120, 255, 190} : Color{220, 90, 90, 220});
        DrawLineEx(scenePos, poiPos, onActivePath ? 3.0f : 1.5f, edgeColor);
        DrawCircleV(poiPos, 7.0f, edgeColor);
        DrawCircleLines(static_cast<int>(poiPos.x), static_cast<int>(poiPos.y), 7.0f,
                        Color{235, 220, 255, 240});

        const Rectangle titleRect = poiEdge.collisionRects.empty()
            ? Rectangle{poiPos.x, poiPos.y, 0.0f, 0.0f}
            : poiEdge.collisionRects.front();
        const int poiLabelX = static_cast<int>(titleRect.x) + 6;
        const int poiLabelY = static_cast<int>(titleRect.y) + 24;
        DrawText(poiEdge.label.c_str(), poiLabelX, poiLabelY, 12, Color{224, 205, 255, 245});
    }

    Color nodeColor = nodeLevelColor(currentSceneId);
    if (isCurrentScene) nodeColor = lightenColor(nodeColor, 24);
    if (nodeBlocked) nodeColor = Color{230, 90, 90, 240};
    DrawCircleV(scenePos, 10.0f, nodeColor);
    DrawCircleLines(static_cast<int>(scenePos.x), static_cast<int>(scenePos.y), 10.0f, WHITE);
    DrawText(sceneNode.name.c_str(), static_cast<int>(scenePos.x) + 12, static_cast<int>(scenePos.y) - 10,
             14, Color{230, 245, 255, 240});
}
} // namespace

void UIManager::handleInput(const InputState& input, State& state) const {
    if (input.toggleInfoMenu) {
        state.infoMenuOpen = !state.infoMenuOpen;
    }
    if (input.toggleInterestZones) {
        state.showInterestZones = !state.showInterestZones;
    }
}

void UIManager::refreshTraversalViews(State& state,
                                      const std::string& currentSceneId,
                                      NavigationService& navService,
                                      bool mobilityReduced) const {
    state.dfsTraversalView = navService.runDfs(currentSceneId, mobilityReduced);
    state.bfsTraversalView = navService.runBfs(currentSceneId, mobilityReduced);
}

void UIManager::renderWorld(const RenderContext& ctx,
                            const CampusGraph& graph,
                            const std::vector<SceneLink>& sceneLinks,
                            const DestinationCatalog& destinationCatalog,
                            const std::vector<std::string>& blockedNodes,
                            bool mobilityReduced,
                            const TransitionService& transitions) const {
    if (ctx.showNavigationGraph && ctx.mapData.hasTexture) {
        const std::vector<std::string> highlightedPathNodes =
            ctx.routeActive && ctx.routeScenePlan && !ctx.routeScenePlan->empty()
                ? *ctx.routeScenePlan
                : std::vector<std::string>{};
        drawCurrentSceneNavigationOverlay(graph, ctx.currentSceneName, sceneLinks, destinationCatalog,
                                          highlightedPathNodes, blockedNodes, mobilityReduced);

        if (ctx.routeActive && ctx.routePathScene && ctx.routePathPoints &&
            *ctx.routePathScene == ctx.currentSceneName && ctx.routePathPoints->size() >= 2) {
            const float pulse = 2.8f + std::sin(static_cast<float>(GetTime()) * 3.0f) * 0.8f;
            const Vector2 routeStart = routeLeadAnchor(ctx.playerPos, (*ctx.routePathPoints)[1]);
            for (size_t i = 1; i < ctx.routePathPoints->size(); ++i) {
                const Vector2 a = (i == 1) ? routeStart : (*ctx.routePathPoints)[i - 1];
                DrawLineEx(a, (*ctx.routePathPoints)[i], pulse, Color{170, 95, 255, 235});
            }
        }
        if (ctx.dfsOverlayPathPoints && ctx.dfsOverlayPathPoints->size() >= 2) {
            for (size_t i = 1; i < ctx.dfsOverlayPathPoints->size(); ++i) {
                DrawLineEx((*ctx.dfsOverlayPathPoints)[i - 1], (*ctx.dfsOverlayPathPoints)[i], 3.0f,
                           Color{70, 255, 160, 220});
            }
        }
        if (ctx.alternateOverlayPathPoints && ctx.alternateOverlayPathPoints->size() >= 2) {
            for (size_t i = 1; i < ctx.alternateOverlayPathPoints->size(); ++i) {
                DrawLineEx((*ctx.alternateOverlayPathPoints)[i - 1], (*ctx.alternateOverlayPathPoints)[i], 3.0f,
                           Color{255, 120, 120, 220});
            }
        }
    }

    if (ctx.showTriggers) {
        for (const auto& portal : transitions.getPortals()) {
            auto drawZone = [](const Rectangle& r) {
                DrawRectangleRec(r, Color{0, 255, 0, 60});
                DrawRectangleLinesEx(r, 1.5f, Color{0, 255, 0, 180});
            };
            if (portal.sceneA == ctx.currentSceneName) drawZone(portal.triggerA);
            if (portal.sceneB == ctx.currentSceneName) drawZone(portal.triggerB);
        }
        for (const auto& uniPortal : transitions.getUniPortals()) {
            if (uniPortal.scene == ctx.currentSceneName) {
                DrawRectangleRec(uniPortal.triggerRect, Color{0, 255, 120, 60});
                DrawRectangleLinesEx(uniPortal.triggerRect, 1.5f, Color{0, 255, 120, 200});
            }
        }
        for (const auto& elev : transitions.getElevators()) {
            if (elev.scene == ctx.currentSceneName) {
                DrawRectangleRec(elev.triggerRect, Color{0, 180, 255, 60});
                DrawRectangleLinesEx(elev.triggerRect, 1.5f, Color{0, 180, 255, 180});
            }
        }
    }
}

void UIManager::renderScreen(const RenderContext& ctx,
                             State& state,
                             RouteRuntimeState& routeState,
                             const std::vector<std::pair<std::string, std::string>>& routeScenes,
                             const std::function<std::string(const std::string&)>& sceneDisplayName,
                             const CampusGraph& graph,
                             TabManagerState& tabState,
                             NavigationService& navService,
                             ScenarioManager& scenarioManager,
                             ComplexityAnalyzer& complexityAnalyzer,
                             RuntimeBlockerService& runtimeBlockerService,
                             const DestinationCatalog& destinationCatalog,
                             MusicService& musicService,
                             SoundEffectService& soundEffectService,
                             ResilienceService& resilienceService,
                             TransitionService& transitions,
                             const std::unordered_map<std::string, SceneData>& sceneDataMap) const {
    if (ctx.mapData.hasTexture && !state.infoMenuOpen) {
        renderMinimap(ctx, runtimeBlockerService.collisionRectsForScene(ctx.currentSceneName));
    }

    transitions.drawFadeOverlay(ctx.screenWidth, ctx.screenHeight);
    renderPrompt(transitions, ctx.screenWidth, ctx.screenHeight);
    renderCoordinateDisplay(ctx.playerPos, ctx.screenWidth);
    DrawText("M: Menu", 16, 12, 20, Color{220, 230, 255, 220});
    DrawText("TAB: POIs", 16, 34, 18, Color{255, 215, 120, 220});

    renderNavigationOverlayMenu(state.showNavigationGraph, state.infoMenuOpen, ctx.currentSceneName,
                                scenarioManager.isMobilityReduced(), scenarioManager.getStudentType(),
                                routeState.routeActive, routeState.routeProgressPct,
                                routeState.routeTotalDistanceMeters, routeState.routeRemainingMeters,
                                resilienceService.isStillConnected(), tabState,
                                resilienceService.getBlockedNodes());

    renderInfoMenu(ctx, state, routeState, routeScenes, sceneDisplayName, graph, tabState,
                   navService, scenarioManager, complexityAnalyzer, runtimeBlockerService,
                   destinationCatalog, musicService, soundEffectService, resilienceService);

    if (!state.infoMenuOpen) {
        rlImGuiBegin();
        constexpr bool kEnableLegacyImGuiGraphMenu = false;
        if (kEnableLegacyImGuiGraphMenu) {
            renderLegacyImGuiOverlay(state, tabState, navService, scenarioManager, complexityAnalyzer,
                                     resilienceService, graph, sceneDataMap, routeScenes,
                                     sceneDisplayName, ctx.currentSceneName, routeState);
        }
        transitions.drawFloorMenu();
        rlImGuiEnd();
    }
}

void UIManager::renderMinimap(const RenderContext& ctx, const std::vector<Rectangle>& blockedRects) const {
    constexpr int kMapW = 200;
    constexpr int kMapH = 150;
    constexpr int kMapPad = 12;
    constexpr float kWorldRadius = 300.0f;

    const int mapX = ctx.screenWidth - kMapW - kMapPad;
    const int mapY = ctx.screenHeight - kMapH - kMapPad;

    const float texW = static_cast<float>(ctx.mapData.texture.width);
    const float texH = static_cast<float>(ctx.mapData.texture.height);
    const float srcW = std::min(2.0f * kWorldRadius, texW);
    const float srcH = std::min(2.0f * kWorldRadius, texH);
    const float srcX = std::clamp(ctx.playerPos.x - srcW * 0.5f, 0.0f, std::max(0.0f, texW - srcW));
    const float srcY = std::clamp(ctx.playerPos.y - srcH * 0.5f, 0.0f, std::max(0.0f, texH - srcH));
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

    DrawRectangle(mapX - 2, mapY - 2, kMapW + 4, kMapH + 4, Color{0, 0, 0, 200});
    DrawTexturePro(ctx.mapData.texture,
                   srcRect,
                   Rectangle{static_cast<float>(mapX), static_cast<float>(mapY),
                             static_cast<float>(kMapW), static_cast<float>(kMapH)},
                   Vector2{0, 0},
                   0.0f,
                   Color{255, 255, 255, 210});

    for (const auto& hitbox : ctx.mapData.hitboxes) {
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

    for (const auto& blockedRect : blockedRects) {
        const float left = std::max(blockedRect.x, srcRect.x);
        const float top = std::max(blockedRect.y, srcRect.y);
        const float right = std::min(blockedRect.x + blockedRect.width, srcRect.x + srcRect.width);
        const float bottom = std::min(blockedRect.y + blockedRect.height, srcRect.y + srcRect.height);
        if (right <= left || bottom <= top) continue;

        DrawRectangleRec(Rectangle{
            static_cast<float>(mapX) + ((left - srcRect.x) / srcRect.width) * kMapW,
            static_cast<float>(mapY) + ((top - srcRect.y) / srcRect.height) * kMapH,
            ((right - left) / srcRect.width) * kMapW,
            ((bottom - top) / srcRect.height) * kMapH
        }, Color{220, 70, 70, 145});
    }

    if (ctx.routeActive && ctx.routePathScene && ctx.routePathPoints &&
        *ctx.routePathScene == ctx.currentSceneName && ctx.routePathPoints->size() >= 2) {
        const Vector2 routeStart = routeLeadAnchor(ctx.playerPos, (*ctx.routePathPoints)[1]);
        for (size_t i = 1; i < ctx.routePathPoints->size(); ++i) {
            const Vector2 worldA = (i == 1) ? routeStart : (*ctx.routePathPoints)[i - 1];
            const Vector2 a = clampMiniPoint(worldToMini(worldA));
            const Vector2 b = clampMiniPoint(worldToMini((*ctx.routePathPoints)[i]));
            DrawLineEx(a, b, 3.0f, Color{170, 95, 255, 240});
        }
        const Vector2 goalMarker = clampMiniPoint(worldToMini(ctx.routePathPoints->back()));
        DrawCircleV(goalMarker, 5.0f, Color{184, 120, 255, 240});
        DrawCircleLines(static_cast<int>(goalMarker.x), static_cast<int>(goalMarker.y), 5.0f, BLACK);
    }

    const Vector2 playerMini = clampMiniPoint(worldToMini(ctx.playerPos));
    DrawCircle(static_cast<int>(playerMini.x), static_cast<int>(playerMini.y), 4.0f, Color{0, 220, 255, 255});
    DrawCircleLines(static_cast<int>(playerMini.x), static_cast<int>(playerMini.y), 4.0f, WHITE);

    DrawRectangleLines(mapX - 2, mapY - 2, kMapW + 4, kMapH + 4, Color{80, 160, 255, 200});
    DrawText("Map", mapX + 4, mapY + 4, 12, Color{180, 220, 255, 220});
    if (ctx.routeActive) {
        DrawText("Route active", mapX + 52, mapY + 4, 12, Color{214, 180, 255, 220});
    }
}

void UIManager::renderCoordinateDisplay(const Vector2& playerPos, int screenWidth) const {
    const char* coordText = TextFormat("Pos: (%.1f, %.1f)", playerPos.x, playerPos.y);
    const int coordFontSize = 20;
    const int coordPadding = 12;
    const int coordWidth = MeasureText(coordText, coordFontSize);
    const int coordX = screenWidth - coordWidth - coordPadding;
    const int coordY = coordPadding;
    DrawRectangle(coordX - 8, coordY - 6, coordWidth + 16, coordFontSize + 12, Color{0, 0, 0, 140});
    DrawText(coordText, coordX, coordY, coordFontSize, RAYWHITE);
}

void UIManager::renderPrompt(const TransitionService& transitions, int screenWidth, int screenHeight) const {
    if (!transitions.isPromptVisible()) return;

    const std::string hintText = transitions.getPromptHint();
    const char* hint = hintText.c_str();
    const int hintFontSz = 22;
    const int hintW = MeasureText(hint, hintFontSz);
    const int hintX = (screenWidth - hintW) / 2;
    const int hintY = screenHeight - 60;
    DrawRectangle(hintX - 10, hintY - 8, hintW + 20, hintFontSz + 16, Color{0, 0, 0, 180});
    DrawText(hint, hintX, hintY, hintFontSz, YELLOW);
}

void UIManager::renderNavigationOverlayMenu(bool showNavigationGraph,
                                            bool infoMenuOpen,
                                            const std::string& currentSceneName,
                                            bool mobilityReduced,
                                            StudentType studentType,
                                            bool routeActive,
                                            float routeProgressPct,
                                            float routeTotalDistanceMeters,
                                            float routeRemainingMeters,
                                            bool resilienceConnected,
                                            const TabManagerState& state,
                                            const std::vector<std::string>& blockedNodes) const {
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
    DrawText(TextFormat("Profile: %s", studentTypeToLabel(studentType)), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Mobility reduced: %s", mobilityReduced ? "ON" : "OFF"), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Connectivity: %s", resilienceConnected ? "connected" : "fragmented"),
             x + 12, cy, 21, resilienceConnected ? Color{150, 238, 180, 255} : Color{255, 160, 160, 255}); cy += 22;
    DrawText(TextFormat("Route: %s", routeActive ? "active" : "inactive"), x + 12, cy, 21, RAYWHITE); cy += 22;
    DrawText(TextFormat("Route progress: %.1f%%", routeProgressPct), x + 12, cy, 21, RAYWHITE); cy += 22;
    if (routeActive) {
        DrawText(TextFormat("Total distance: %.1f m", routeTotalDistanceMeters),
                 x + 12, cy, 21, Color{200, 230, 255, 255}); cy += 22;
        DrawText(TextFormat("Remaining: %.1f m", routeRemainingMeters),
                 x + 12, cy, 21, Color{255, 220, 100, 255}); cy += 22;
    }
    DrawText(TextFormat("Blocked nodes: %d", static_cast<int>(blockedNodes.size())), x + 12, cy, 21, Color{255, 224, 170, 255}); cy += 22;
    if (state.hasPath) {
        DrawText(TextFormat("Last path weight: %.2f", state.lastPath.total_weight), x + 12, cy, 21, Color{200, 225, 255, 255});
    }
}

void UIManager::renderInfoMenu(const RenderContext& ctx,
                               State& state,
                               RouteRuntimeState& routeState,
                               const std::vector<std::pair<std::string, std::string>>& routeScenes,
                               const std::function<std::string(const std::string&)>& sceneDisplayName,
                               const CampusGraph& graph,
                               TabManagerState& tabState,
                               NavigationService& navService,
                               ScenarioManager& scenarioManager,
                               ComplexityAnalyzer& complexityAnalyzer,
                               RuntimeBlockerService& runtimeBlockerService,
                               const DestinationCatalog& destinationCatalog,
                               MusicService& musicService,
                               SoundEffectService& soundEffectService,
                               ResilienceService& resilienceService) const {
    if (!state.infoMenuOpen) return;

    const float uiScale = std::clamp(static_cast<float>(ctx.screenHeight) / 900.0f, 1.0f, 1.45f);
    const auto px = [uiScale](int base) {
        return std::max(1, static_cast<int>(std::round(static_cast<float>(base) * uiScale)));
    };

    const int titleFont = px(22);
    const int sectionTitleFont = px(24);
    const int bodyFont = px(17);
    const int bodyMutedFont = px(17);
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
    const Color accentDfs = Color{255, 184, 116, 255};
    const Color accentBfs = Color{114, 188, 255, 255};
    const Color good = Color{100, 220, 100, 255};
    const Color bad = Color{220, 100, 100, 255};
    const auto blockedNodes = resilienceService.getBlockedNodes();
    const auto blockedEdges = resilienceService.getBlockedEdges();
    const auto nodeIds = graph.nodeIds();

    auto setBuffer = [](char* buffer, size_t bufferSize, const std::string& value) {
        std::snprintf(buffer, bufferSize, "%s", value.c_str());
    };

    auto ensureNodeBuffer = [&](char* buffer, size_t bufferSize) -> int {
        if (nodeIds.empty()) {
            buffer[0] = '\0';
            return -1;
        }

        for (size_t i = 0; i < nodeIds.size(); ++i) {
            if (nodeIds[i] == buffer) return static_cast<int>(i);
        }

        setBuffer(buffer, bufferSize, nodeIds.front());
        return 0;
    };

    auto nodeDisplayName = [&](const std::string& nodeId) -> std::string {
        if (!graph.hasNode(nodeId)) return nodeId;
        const auto& node = graph.getNode(nodeId);
        return node.name.empty() ? nodeId : (nodeId + " - " + node.name);
    };

    auto drawPagedLines = [&](const std::vector<std::string>& lines,
                              int& page,
                              int x,
                              int& y,
                              int width,
                              int listHeight) {
        const int lineHeight = px(20);
        const int visibleLines = std::max(1, listHeight / lineHeight);
        const int totalPages = std::max(1, static_cast<int>((lines.size() + visibleLines - 1) / visibleLines));
        page = std::clamp(page, 0, totalPages - 1);

        Rectangle listBg{static_cast<float>(x), static_cast<float>(y),
                         static_cast<float>(width), static_cast<float>(listHeight)};
        DrawRectangleRec(listBg, Color{14, 26, 42, 220});
        DrawRectangleLinesEx(listBg, 1.5f, Color{70, 100, 138, 220});

        const int startIdx = page * visibleLines;
        const int endIdx = std::min(static_cast<int>(lines.size()), startIdx + visibleLines);
        int lineY = y + px(8);
        for (int i = startIdx; i < endIdx; ++i) {
            DrawText(lines[i].c_str(), x + px(8), lineY, bodyMutedFont, muted);
            lineY += lineHeight;
        }
        y += listHeight + px(10);

        Rectangle prevBtn{static_cast<float>(x), static_cast<float>(y),
                          static_cast<float>(px(36)), static_cast<float>(px(28))};
        Rectangle nextBtn{static_cast<float>(x + px(132)), static_cast<float>(y),
                          static_cast<float>(px(36)), static_cast<float>(px(28))};
        if (drawRayButton(prevBtn, "<", bodyFont, btn, btnHover, btnActive, white)) {
            page = std::max(0, page - 1);
        }
        if (drawRayButton(nextBtn, ">", bodyFont, btn, btnHover, btnActive, white)) {
            page = std::min(totalPages - 1, page + 1);
        }
        DrawText(TextFormat("Pagina %d/%d", page + 1, totalPages),
                 x + px(46), y + px(5), bodyMutedFont, white);
        y += px(38);
    };

    const int startIndex = ensureNodeBuffer(tabState.startId, sizeof(tabState.startId));
    const int endIndex = ensureNodeBuffer(tabState.endId, sizeof(tabState.endId));
    const int nodeIndex = ensureNodeBuffer(tabState.nodeId, sizeof(tabState.nodeId));
    (void)startIndex;
    (void)endIndex;
    (void)nodeIndex;

    DrawRectangle(0, 0, ctx.screenWidth, ctx.screenHeight, Color{0, 0, 0, 0});

    const int margin = panelMargin;
    const int topH = topBarHeight;
    Rectangle topBar{0.0f, 0.0f, static_cast<float>(ctx.screenWidth), static_cast<float>(topH)};
    DrawRectangleRec(topBar, Color{26, 62, 115, 245});

    DrawText("Information Menu (Raylib)", margin, px(12), titleFont, white);
    Rectangle closeBtn{static_cast<float>(ctx.screenWidth - margin - px(24)), static_cast<float>(px(4)),
                       static_cast<float>(px(32)), static_cast<float>(px(32))};
    if (drawRayButton(closeBtn, "X", titleFont, Color{26, 62, 115, 245}, Color{36, 81, 148, 255},
                      Color{60, 95, 155, 255}, white)) {
        soundEffectService.play(SoundEffectType::SelectButton);
        state.infoMenuOpen = false;
        return;
    }

    auto closeAllDropdowns = [&]() {
        state.routeDropdownOpen = false;
        state.startNodeDropdownOpen = false;
        state.endNodeDropdownOpen = false;
        state.blockedNodeDropdownOpen = false;
        state.blockedEdgeDropdownOpen = false;
        state.menuTabDropdownOpen = false;
    };

    auto focusDropdownSelection = [&](int optionCount, int visibleCount, int selectedIndex, int& scrollIndex) {
        const int safeVisible = std::max(1, visibleCount);
        const int maxScroll = std::max(0, optionCount - safeVisible);
        const int centered = std::max(0, selectedIndex - safeVisible / 2);
        scrollIndex = std::clamp(centered, 0, maxScroll);
    };

    auto drawScrollableDropdown = [&](const std::vector<std::string>& options,
                                      int& selectedIndex,
                                      bool& dropdownOpen,
                                      int& scrollIndex,
                                      int x,
                                      int& y,
                                      int width,
                                      const std::string& buttonLabel,
                                      int buttonFont,
                                      int maxVisible = 6) -> bool {
        Rectangle selectorBtn{static_cast<float>(x), static_cast<float>(y),
                              static_cast<float>(width), static_cast<float>(buttonHeight)};
        if (drawRayButton(selectorBtn, buttonLabel.c_str(), buttonFont, btn, btnHover, btnActive, white)) {
            const bool nextOpen = !dropdownOpen;
            closeAllDropdowns();
            dropdownOpen = nextOpen;
            if (dropdownOpen) {
                focusDropdownSelection(static_cast<int>(options.size()), maxVisible, selectedIndex, scrollIndex);
            }
            soundEffectService.play(SoundEffectType::SelectButton);
        }
        y += px(46);

        if (!dropdownOpen || options.empty()) return false;

        selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(options.size()) - 1);

        const int visibleCount = std::min(static_cast<int>(options.size()), maxVisible);
        const int maxScroll = std::max(0, static_cast<int>(options.size()) - visibleCount);
        scrollIndex = std::clamp(scrollIndex, 0, maxScroll);

        Rectangle dropdownBg{static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(width),
                             static_cast<float>(visibleCount * px(34) + px(30))};
        if (CheckCollisionPointRec(GetMousePosition(), dropdownBg)) {
            const int wheelSteps = static_cast<int>(std::round(GetMouseWheelMove()));
            if (wheelSteps != 0) {
                scrollIndex = std::clamp(scrollIndex - wheelSteps, 0, maxScroll);
            }
        }

        DrawRectangleRec(dropdownBg, Color{14, 28, 48, 245});
        DrawRectangleLinesEx(dropdownBg, 1.5f, Color{96, 112, 180, 220});

        const int startIdx = scrollIndex;
        const int endIdx = std::min(static_cast<int>(options.size()), startIdx + visibleCount);
        for (int i = startIdx; i < endIdx; ++i) {
            const int visualIndex = i - startIdx;
            Rectangle optionRect{dropdownBg.x + px(4), dropdownBg.y + px(4) + visualIndex * px(34),
                                 dropdownBg.width - px(8), static_cast<float>(px(30))};
            const bool selected = selectedIndex == i;
            if (drawRayButton(optionRect, options[i].c_str(), bodyMutedFont,
                              selected ? btnActive : Color{24, 40, 64, 255}, btnHover, btnActive, white)) {
                selectedIndex = i;
                dropdownOpen = false;
                soundEffectService.play(SoundEffectType::BetweenOptions);
                return true;
            }
        }

        const std::string footer = TextFormat("%d-%d / %d", startIdx + 1, endIdx, static_cast<int>(options.size()));
        DrawText(footer.c_str(), x + px(8), y + visibleCount * px(34) + px(6), bodyMutedFont, muted);
        if (maxScroll > 0) {
            DrawText("Scroll", x + width - px(54), y + visibleCount * px(34) + px(6), bodyMutedFont, muted);
        }
        y += visibleCount * px(34) + px(34);
        return false;
    };

    auto drawNodeSelector = [&](const char* label,
                                char* buffer,
                                size_t bufferSize,
                                bool& dropdownOpen,
                                int& dropdownScroll,
                                int x,
                                int& y,
                                int width) {
        DrawText(label, x, y, bodyFont, white);
        y += px(22);

        const int currentIndex = ensureNodeBuffer(buffer, bufferSize);
        if (currentIndex < 0) {
            DrawText("No hay nodos disponibles.", x, y, bodyMutedFont, muted);
            y += px(28);
            return;
        }

        const std::string display = nodeDisplayName(buffer);
        const std::string selectorLabel = display + (dropdownOpen ? "  ^" : "  v");
        std::vector<std::string> nodeOptions;
        nodeOptions.reserve(nodeIds.size());
        for (const auto& nodeId : nodeIds) {
            nodeOptions.push_back(nodeDisplayName(nodeId));
        }

        int selectedIndex = currentIndex;
        if (drawScrollableDropdown(nodeOptions, selectedIndex, dropdownOpen, dropdownScroll,
                                   x, y, width, selectorLabel, bodyMutedFont)) {
            setBuffer(buffer, bufferSize, nodeIds[selectedIndex]);
        }
    };

    Rectangle audioToggleBtn{static_cast<float>(ctx.screenWidth - margin - px(160)), static_cast<float>(px(4)),
                             static_cast<float>(px(110)), static_cast<float>(px(32))};
    const std::string audioToggleLabel = std::string("Audio ") + (state.audioPanelExpanded ? "^" : "v");
    if (drawRayButton(audioToggleBtn, audioToggleLabel.c_str(), bodyMutedFont,
                      Color{26, 62, 115, 245}, Color{36, 81, 148, 255}, Color{60, 95, 155, 255}, white)) {
        state.audioPanelExpanded = !state.audioPanelExpanded;
        soundEffectService.play(SoundEffectType::SelectButton);
    }

    const int audioPanelH = state.audioPanelExpanded ? px(78) : 0;
    if (state.audioPanelExpanded) {
        Rectangle audioPanel{static_cast<float>(margin), static_cast<float>(topH + margin),
                             static_cast<float>(ctx.screenWidth - margin * 2), static_cast<float>(audioPanelH)};
        DrawRectangleRec(audioPanel, Color{14, 22, 36, 230});
        DrawRectangleLinesEx(audioPanel, 2.0f, border);

        const float musicVolume = AudioManager::getInstance().getMusicVolume();
        const float sfxVolume = AudioManager::getInstance().getSFXVolume();
        float newMusicVolume = musicVolume;
        float newSfxVolume = sfxVolume;

        DrawText("Audio", margin + sectionPad, static_cast<int>(audioPanel.y) + px(10), sectionTitleFont, white);
        DrawText(TextFormat("Perfil: %s | Movilidad: %s",
                            studentTypeToLabel(scenarioManager.getStudentType()),
                            scenarioManager.isMobilityReduced() ? "reducida" : "normal"),
                 margin + px(130), static_cast<int>(audioPanel.y) + px(14), bodyMutedFont, muted);

        const int sliderY = static_cast<int>(audioPanel.y) + px(42);
        DrawText("Musica", margin + sectionPad, sliderY - px(2), bodyFont, white);
        Rectangle musicSlider{static_cast<float>(margin + px(98)), static_cast<float>(sliderY),
                              static_cast<float>(px(240)), static_cast<float>(px(16))};
        if (drawHorizontalSlider(musicSlider, newMusicVolume, 0.0f, 1.0f,
                                 Color{34, 45, 68, 255}, Color{136, 92, 255, 255}, white)) {
            musicService.setVolume(newMusicVolume);
        }
        DrawText(TextFormat("%d%%", static_cast<int>(std::round(newMusicVolume * 100.0f))),
                 static_cast<int>(musicSlider.x + musicSlider.width + px(10)),
                 sliderY - px(2), bodyMutedFont, muted);

        const int sfxLabelX = margin + px(420);
        DrawText("SFX", sfxLabelX, sliderY - px(2), bodyFont, white);
        Rectangle sfxSlider{static_cast<float>(sfxLabelX + px(52)), static_cast<float>(sliderY),
                            static_cast<float>(px(240)), static_cast<float>(px(16))};
        if (drawHorizontalSlider(sfxSlider, newSfxVolume, 0.0f, 1.0f,
                                 Color{34, 45, 68, 255}, Color{88, 196, 255, 255}, white)) {
            soundEffectService.setVolume(newSfxVolume);
        }
        DrawText(TextFormat("%d%%", static_cast<int>(std::round(newSfxVolume * 100.0f))),
                 static_cast<int>(sfxSlider.x + sfxSlider.width + px(10)),
                 sliderY - px(2), bodyMutedFont, muted);
    }

    const int contentY = topH + margin + audioPanelH + (state.audioPanelExpanded ? margin : 0);
    const int contentH = ctx.screenHeight - contentY - margin;
    const int leftW = static_cast<int>(ctx.screenWidth * 0.28f);
    const int rightX = margin + leftW + margin;
    const int rightW = ctx.screenWidth - rightX - margin;
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

    Rectangle routeSelectBtn{static_cast<float>(margin + sectionPad), static_cast<float>(yLeft),
                             static_cast<float>(leftW - sectionPad * 2), static_cast<float>(buttonHeight)};

    std::string selectedLabel = "(sin destinos)";
    if (!routeScenes.empty()) {
        routeState.selectedDestinationIdx =
            std::clamp(routeState.selectedDestinationIdx, 0, static_cast<int>(routeScenes.size()) - 1);
        selectedLabel = routeScenes[routeState.selectedDestinationIdx].second;
    }

    std::string routeSelectLabel = "Destino: " + selectedLabel + (state.routeDropdownOpen ? "  ^" : "  v");
    if (!routeScenes.empty()) {
        std::vector<std::string> routeLabels;
        routeLabels.reserve(routeScenes.size());
        for (const auto& routeScene : routeScenes) {
            routeLabels.push_back(routeScene.second);
        }
        drawScrollableDropdown(routeLabels, routeState.selectedDestinationIdx,
                               state.routeDropdownOpen, state.routeDropdownScroll,
                               margin + sectionPad, yLeft, leftW - sectionPad * 2,
                               routeSelectLabel, bodyFont);
    } else {
        if (drawRayButton(routeSelectBtn, routeSelectLabel.c_str(), bodyFont, btn, btnHover, btnActive, white)) {
            soundEffectService.play(SoundEffectType::SelectButton);
        }
        yLeft += px(46);
    }

    Rectangle drawRouteBtn{static_cast<float>(margin + sectionPad), static_cast<float>(yLeft),
                           static_cast<float>(px(140)), static_cast<float>(buttonHeight)};
    Rectangle clearBtn{static_cast<float>(margin + sectionPad + px(150)), static_cast<float>(yLeft),
                       static_cast<float>(px(110)), static_cast<float>(buttonHeight)};
    if (!routeScenes.empty() &&
        drawRayButton(drawRouteBtn, "Draw Route", bodyFont, btn, btnHover, btnActive, white)) {
        routeState.routeActive = true;
        routeState.routeTargetNodeId = routeScenes[routeState.selectedDestinationIdx].first;
        routeState.routeProgressPct = 0.0f;
        routeState.routeTravelElapsed = 0.0f;
        routeState.routeTravelCompleted = false;
        routeState.routeLegStartDistance = 0.0f;
        routeState.routeLegSceneId.clear();
        routeState.routeLegNextSceneId.clear();
        routeState.routeRefreshCooldown = 0.0f;
        soundEffectService.play(SoundEffectType::RouteFixated);
    }
    if (drawRayButton(clearBtn, "Clear", bodyFont, btn, btnHover, btnActive, white)) {
        routeState.routeActive = false;
        routeState.routeTargetNodeId.clear();
        routeState.routeProgressPct = 0.0f;
        routeState.routeTravelElapsed = 0.0f;
        routeState.routeTravelCompleted = false;
        routeState.routeLegStartDistance = 0.0f;
        routeState.routeLegSceneId.clear();
        routeState.routeLegNextSceneId.clear();
        routeState.routeScenePlan.clear();
        routeState.routePathPoints.clear();
        routeState.routeNextHint.clear();
        tabState.lastPath = {};
        tabState.hasPath = false;
        soundEffectService.play(SoundEffectType::SelectButton);
    }
    yLeft += px(52);

    if (routeState.routeActive) {
        DrawText(TextFormat("Destination: %s", destinationCatalog.displayLabel(routeState.routeTargetNodeId).c_str()),
                 margin + sectionPad, yLeft, bodyFont, white);
        yLeft += px(24);
        DrawText(TextFormat("Distancia total: %.1f m", routeState.routeTotalDistanceMeters),
                 margin + sectionPad, yLeft, bodyMutedFont, Color{200, 230, 255, 255});
        yLeft += px(22);
        DrawText(TextFormat("Distancia restante: %.1f m", routeState.routeRemainingMeters),
                 margin + sectionPad, yLeft, bodyMutedFont, Color{255, 220, 100, 255});
        yLeft += px(24);
        DrawText(routeState.routeNextHint.c_str(), margin + sectionPad, yLeft, bodyMutedFont, muted);
        yLeft += px(28);
        DrawText("Scene plan:", margin + sectionPad, yLeft, bodyFont, white);
        yLeft += px(22);
        for (const auto& sceneId : routeState.routeScenePlan) {
            const std::string item = "- " + sceneDisplayName(sceneId);
            DrawText(item.c_str(), margin + sectionPad + px(6), yLeft, bodyMutedFont, muted);
            yLeft += px(20);
            if (yLeft > contentY + contentH - px(24)) break;
        }
    }

    int yRight = contentY + sectionPad;
    DrawText("Academic Analysis", rightX + sectionPad, yRight, sectionTitleFont, white);
    yRight += px(32);
    DrawText(TextFormat("Escena actual: %s", sceneDisplayName(ctx.currentSceneName).c_str()),
             rightX + sectionPad, yRight, bodyMutedFont, muted);
    yRight += px(28);

    const std::vector<std::string> menuTabs{
        "Mapa", "DFS", "BFS", "Conexo", "Camino", "Escenarios", "Complejidad", "Fallos"
    };
    state.activeMenuTab = std::clamp(state.activeMenuTab, 0, static_cast<int>(menuTabs.size()) - 1);
    DrawText("Seccion:", rightX + sectionPad, yRight, bodyFont, white);
    yRight += px(22);
    const std::string tabDropdownLabel =
        menuTabs[state.activeMenuTab] + (state.menuTabDropdownOpen ? "  ^" : "  v");
    const int previousTab = state.activeMenuTab;
    if (drawScrollableDropdown(menuTabs, state.activeMenuTab,
                               state.menuTabDropdownOpen, state.menuTabDropdownScroll,
                               rightX + sectionPad, yRight, rightW - sectionPad * 2,
                               tabDropdownLabel, bodyMutedFont, 7)) {
        if (state.activeMenuTab != previousTab) {
            state.analysisScroll = 0.0f;
            closeAllDropdowns();
        }
    }
    yRight += px(6);

    const int contentX = rightX + sectionPad;
    const int contentW = rightW - sectionPad * 2;
    const int rightViewportTop = yRight;
    const int rightViewportHeight = std::max(px(180), contentH - (rightViewportTop - contentY) - sectionPad);
    const Rectangle rightViewport{static_cast<float>(contentX), static_cast<float>(rightViewportTop),
                                  static_cast<float>(contentW), static_cast<float>(rightViewportHeight)};
    const bool anyDropdownOpen = state.routeDropdownOpen || state.startNodeDropdownOpen ||
                                 state.endNodeDropdownOpen || state.blockedNodeDropdownOpen ||
                                 state.blockedEdgeDropdownOpen || state.menuTabDropdownOpen;
    if (!anyDropdownOpen && CheckCollisionPointRec(GetMousePosition(), rightViewport)) {
        const float wheel = GetMouseWheelMove();
        if (std::fabs(wheel) > 0.01f) {
            state.analysisScroll = std::max(0.0f, state.analysisScroll - wheel * static_cast<float>(px(34)));
        }
    }
    const int scrollBaseY = yRight - static_cast<int>(std::round(state.analysisScroll));
    yRight = scrollBaseY;

    BeginScissorMode(contentX, rightViewportTop, contentW, rightViewportHeight);

    switch (state.activeMenuTab) {
        case 0: {
            DrawText("Mapa del Campus y Grafo", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);

            Rectangle graphToggleBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                                     static_cast<float>(px(260)), static_cast<float>(buttonHeight)};
            if (drawRayButton(graphToggleBtn,
                              state.showNavigationGraph ? "Grafo: VISIBLE" : "Grafo: OCULTO",
                              bodyFont, btn, btnHover, btnActive, white)) {
                state.showNavigationGraph = !state.showNavigationGraph;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            yRight += px(46);

            DrawText(TextFormat("Nodos totales: %d", graph.nodeCount()), contentX, yRight, bodyFont, white);
            yRight += px(22);
            DrawText(TextFormat("Aristas totales: %d", graph.edgeCount()), contentX, yRight, bodyFont, white);
            yRight += px(22);
            DrawText(TextFormat("Nodos bloqueados: %d", static_cast<int>(blockedNodes.size())), contentX, yRight, bodyFont, white);
            yRight += px(22);
            DrawText(TextFormat("Conexiones bloqueadas: %d", static_cast<int>(blockedEdges.size())), contentX, yRight, bodyFont, white);
            yRight += px(30);

            std::vector<std::string> graphLines;
            graphLines.reserve(nodeIds.size());
            for (const auto& nodeId : nodeIds) {
                try {
                    const Node& node = graph.getNode(nodeId);
                    graphLines.push_back(TextFormat("[%s] %s | tipo=%s | z=%d",
                                                    nodeId.c_str(), node.name.c_str(),
                                                    node.type.c_str(), static_cast<int>(std::round(node.z))));
                } catch (...) {
                    graphLines.push_back(nodeId);
                }
            }
            DrawText("Lista de nodos:", contentX, yRight, bodyFont, white);
            yRight += px(22);
            drawPagedLines(graphLines, state.graphViewPage, contentX, yRight, contentW, px(220));

            Rectangle legend{static_cast<float>(contentX), static_cast<float>(yRight),
                             static_cast<float>(std::min(contentW, px(360))), static_cast<float>(px(110))};
            DrawRectangleRec(legend, Color{18, 30, 46, 230});
            DrawRectangleLinesEx(legend, 1.5f, Color{90, 125, 165, 220});
            int ly = static_cast<int>(legend.y) + px(10);
            DrawText("Leyenda visual:", static_cast<int>(legend.x) + px(10), ly, bodyFont, white);
            ly += px(24);
            DrawCircle(static_cast<int>(legend.x) + px(18), ly + px(6), 5, Color{90, 180, 255, 235});
            DrawText("Nodo normal", static_cast<int>(legend.x) + px(32), ly, bodyMutedFont, muted);
            ly += px(20);
            DrawCircle(static_cast<int>(legend.x) + px(18), ly + px(6), 5, Color{170, 95, 255, 220});
            DrawText("Nodo destacado en ruta", static_cast<int>(legend.x) + px(32), ly, bodyMutedFont, muted);
            ly += px(20);
            DrawCircle(static_cast<int>(legend.x) + px(18), ly + px(6), 5, Color{230, 90, 90, 240});
            DrawText("Nodo bloqueado", static_cast<int>(legend.x) + px(32), ly, bodyMutedFont, muted);
            break;
        }

        case 1: {
            DrawText("Recorrido DFS", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);
            drawNodeSelector("Nodo de inicio:", tabState.startId, sizeof(tabState.startId),
                             state.startNodeDropdownOpen, state.startNodeDropdownScroll,
                             contentX, yRight, px(320));

            Rectangle execBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                              static_cast<float>(px(180)), static_cast<float>(buttonHeight)};
            if (drawRayButton(execBtn, "Ejecutar DFS", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastDfsTraversal = navService.runDfs(tabState.startId, scenarioManager.isMobilityReduced());
                tabState.lastTraversal = tabState.lastDfsTraversal;
                tabState.hasTraversal = true;
                tabState.hasDfsTraversal = true;
                tabState.lastAction = "DFS";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            yRight += px(46);

            if (tabState.hasDfsTraversal) {
                DrawText(TextFormat("Nodos visitados: %d", tabState.lastDfsTraversal.nodes_visited),
                         contentX, yRight, bodyFont, accentDfs);
                yRight += px(22);
                DrawText(TextFormat("Tiempo: %lld us", tabState.lastDfsTraversal.elapsed_us),
                         contentX, yRight, bodyFont, white);
                yRight += px(28);
                DrawText("Orden de visita:", contentX, yRight, bodyFont, white);
                yRight += px(22);

                std::vector<std::string> lines;
                for (size_t i = 0; i < tabState.lastDfsTraversal.visit_order.size(); ++i) {
                    lines.push_back(TextFormat("%zu. %s", i + 1, tabState.lastDfsTraversal.visit_order[i].c_str()));
                }
                drawPagedLines(lines, state.dfsViewPage, contentX, yRight, contentW, px(220));
            }
            break;
        }

        case 2: {
            DrawText("Recorrido BFS", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);
            drawNodeSelector("Nodo de inicio:", tabState.startId, sizeof(tabState.startId),
                             state.startNodeDropdownOpen, state.startNodeDropdownScroll,
                             contentX, yRight, px(320));

            Rectangle execBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                              static_cast<float>(px(180)), static_cast<float>(buttonHeight)};
            if (drawRayButton(execBtn, "Ejecutar BFS", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastBfsTraversal = navService.runBfs(tabState.startId, scenarioManager.isMobilityReduced());
                tabState.lastTraversal = tabState.lastBfsTraversal;
                tabState.hasTraversal = true;
                tabState.hasBfsTraversal = true;
                tabState.lastAction = "BFS";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            yRight += px(46);

            if (tabState.hasBfsTraversal) {
                DrawText(TextFormat("Nodos visitados: %d", tabState.lastBfsTraversal.nodes_visited),
                         contentX, yRight, bodyFont, accentBfs);
                yRight += px(22);
                DrawText(TextFormat("Tiempo: %lld us", tabState.lastBfsTraversal.elapsed_us),
                         contentX, yRight, bodyFont, white);
                yRight += px(28);
                DrawText("Orden de visita:", contentX, yRight, bodyFont, white);
                yRight += px(22);

                std::vector<std::string> lines;
                for (size_t i = 0; i < tabState.lastBfsTraversal.visit_order.size(); ++i) {
                    lines.push_back(TextFormat("%zu. %s", i + 1, tabState.lastBfsTraversal.visit_order[i].c_str()));
                }
                drawPagedLines(lines, state.bfsViewPage, contentX, yRight, contentW, px(220));
            }
            break;
        }

        case 3: {
            DrawText("Conectividad del Campus", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);
            DrawText("Comprueba si todo el grafo sigue conectado con el estado actual.",
                     contentX, yRight, bodyMutedFont, muted);
            yRight += px(34);

            Rectangle connBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                              static_cast<float>(px(240)), static_cast<float>(buttonHeight)};
            if (drawRayButton(connBtn, "Verificar Conexidad", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastConnected = navService.checkConnectivity();
                tabState.hasConnectivityResult = true;
                tabState.lastAction = "Connectivity";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            yRight += px(48);

            if (tabState.hasConnectivityResult) {
                DrawText(tabState.lastConnected ? "RESULTADO: CAMPUS CONEXO" : "RESULTADO: CAMPUS NO CONEXO",
                         contentX, yRight, bodyFont, tabState.lastConnected ? good : bad);
                yRight += px(28);

                const auto components = navService.getComponents();
                DrawText(TextFormat("Componentes detectadas: %d", static_cast<int>(components.size())),
                         contentX, yRight, bodyFont, white);
                yRight += px(26);

                std::vector<std::string> lines;
                for (size_t i = 0; i < components.size(); ++i) {
                    std::string line = "Componente " + std::to_string(i + 1) + ": ";
                    for (size_t j = 0; j < components[i].size(); ++j) {
                        if (j > 0) line += ", ";
                        line += components[i][j];
                    }
                    lines.push_back(line);
                }
                if (lines.empty()) lines.push_back("No hay componentes para mostrar.");
                drawPagedLines(lines, state.graphViewPage, contentX, yRight, contentW, px(180));
            }
            break;
        }

        case 4: {
            DrawText("Buscar Camino entre Dos Puntos", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);
            drawNodeSelector("Origen:", tabState.startId, sizeof(tabState.startId),
                             state.startNodeDropdownOpen, state.startNodeDropdownScroll,
                             contentX, yRight, px(320));
            drawNodeSelector("Destino:", tabState.endId, sizeof(tabState.endId),
                             state.endNodeDropdownOpen, state.endNodeDropdownScroll,
                             contentX, yRight, px(320));

            Rectangle directBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                                static_cast<float>(px(200)), static_cast<float>(buttonHeight)};
            Rectangle dfsBtn{static_cast<float>(contentX + px(210)), static_cast<float>(yRight),
                             static_cast<float>(px(200)), static_cast<float>(buttonHeight)};
            if (drawRayButton(directBtn, "Ruta Perfilada", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastPath = scenarioManager.buildProfiledPath(graph, tabState.startId, tabState.endId);
                tabState.hasPath = true;
                tabState.lastAction = "PathDijkstra";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            if (drawRayButton(dfsBtn, "Buscar Camino DFS", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastPath = runProfiledDfsPath(graph, navService, scenarioManager, tabState.startId, tabState.endId);
                tabState.hasPath = true;
                tabState.lastAction = "PathDFS";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            yRight += px(48);

            if (tabState.hasPath &&
                (tabState.lastAction == "PathDijkstra" || tabState.lastAction == "PathDFS")) {
                DrawText(tabState.lastPath.found ? "CAMINO ENCONTRADO" : "NO SE ENCONTRO CAMINO",
                         contentX, yRight, bodyFont, tabState.lastPath.found ? good : bad);
                yRight += px(26);
                DrawText(TextFormat("Distancia total: %.2f", tabState.lastPath.total_weight),
                         contentX, yRight, bodyFont, white);
                yRight += px(28);

                std::vector<std::string> pathLines;
                for (size_t i = 0; i < tabState.lastPath.path.size(); ++i) {
                    pathLines.push_back(TextFormat("%zu. %s", i + 1, tabState.lastPath.path[i].c_str()));
                }
                if (pathLines.empty()) pathLines.push_back("No hay nodos en la ruta.");
                drawPagedLines(pathLines, state.graphViewPage, contentX, yRight, contentW, px(200));
            }
            break;
        }

        case 5: {
            DrawText("Simulacion de Escenarios", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);

            const int thirdW = (contentW - px(12)) / 3;
            const bool isNew = scenarioManager.getStudentType() == StudentType::NEW_STUDENT;
            const bool isVet = scenarioManager.getStudentType() == StudentType::VETERAN_STUDENT;
            const bool isDis = scenarioManager.getStudentType() == StudentType::DISABLED_STUDENT;
            Rectangle newBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                             static_cast<float>(thirdW), static_cast<float>(buttonHeight)};
            Rectangle vetBtn{static_cast<float>(contentX + thirdW + px(6)), static_cast<float>(yRight),
                             static_cast<float>(thirdW), static_cast<float>(buttonHeight)};
            Rectangle disBtn{static_cast<float>(contentX + (thirdW + px(6)) * 2), static_cast<float>(yRight),
                             static_cast<float>(thirdW), static_cast<float>(buttonHeight)};
            if (drawRayButton(newBtn, "Nuevo", bodyMutedFont,
                              isNew ? btnActive : btn, btnHover, btnActive, white)) {
                scenarioManager.setStudentType(StudentType::NEW_STUDENT);
                routeState.routeRefreshCooldown = 0.0f;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            if (drawRayButton(vetBtn, "Veterano", bodyMutedFont,
                              isVet ? btnActive : btn, btnHover, btnActive, white)) {
                scenarioManager.setStudentType(StudentType::VETERAN_STUDENT);
                routeState.routeRefreshCooldown = 0.0f;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            if (drawRayButton(disBtn, "Discapacitado", bodyMutedFont,
                              isDis ? btnActive : btn, btnHover, btnActive, white)) {
                scenarioManager.setStudentType(StudentType::DISABLED_STUDENT);
                routeState.routeRefreshCooldown = 0.0f;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            yRight += px(46);

            Rectangle mobilityBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                                  static_cast<float>(px(300)), static_cast<float>(buttonHeight)};
            if (drawRayButton(mobilityBtn,
                              scenarioManager.isMobilityReduced() ? "Movilidad reducida: ACTIVA"
                                                                  : "Movilidad reducida: INACTIVA",
                              bodyFont, btn, btnHover, btnActive, white)) {
                if (scenarioManager.getStudentType() != StudentType::DISABLED_STUDENT) {
                    scenarioManager.setMobilityReduced(!scenarioManager.isMobilityReduced());
                    routeState.routeRefreshCooldown = 0.0f;
                }
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            yRight += px(50);

            DrawText("Comportamiento del perfil:", contentX, yRight, bodyFont, white);
            yRight += px(24);
            if (isNew) {
                DrawText("- Debe pasar por un POI valido sin forzar loops innecesarios.", contentX, yRight, bodyMutedFont, accentDfs);
                yRight += px(22);
            }
            if (isVet && !scenarioManager.isMobilityReduced()) {
                DrawText("- Busca la ruta mas corta disponible.", contentX, yRight, bodyMutedFont, good);
                yRight += px(22);
            }
            if (isDis || scenarioManager.isMobilityReduced()) {
                DrawText("- Evita escaleras y prioriza rutas accesibles.", contentX, yRight, bodyMutedFont, bad);
                yRight += px(22);
            }
            yRight += px(10);

            const auto profiledSteps = scenarioManager.applyProfile(graph, tabState.startId, tabState.endId);
            DrawText("Waypoints aplicados al perfil:", contentX, yRight, bodyFont, white);
            yRight += px(22);
            std::vector<std::string> profileLines;
            for (size_t i = 0; i < profiledSteps.size(); ++i) {
                profileLines.push_back(TextFormat("%zu. %s", i + 1, profiledSteps[i].c_str()));
            }
            if (profileLines.empty()) profileLines.push_back("Selecciona origen y destino para ver el perfil.");
            drawPagedLines(profileLines, state.graphViewPage, contentX, yRight, contentW, px(140));
            break;
        }

        case 6: {
            DrawText("Analisis de Complejidad: BFS vs DFS", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);
            drawNodeSelector("Nodo de inicio:", tabState.startId, sizeof(tabState.startId),
                             state.startNodeDropdownOpen, state.startNodeDropdownScroll,
                             contentX, yRight, px(320));
            drawNodeSelector("Nodo destino:", tabState.endId, sizeof(tabState.endId),
                             state.endNodeDropdownOpen, state.endNodeDropdownScroll,
                             contentX, yRight, px(320));

            Rectangle analyzeBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                                 static_cast<float>(px(280)), static_cast<float>(buttonHeight)};
            if (drawRayButton(analyzeBtn, "Ejecutar Analisis Comparativo", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastStats = complexityAnalyzer.analyze(tabState.startId, scenarioManager.isMobilityReduced());
                tabState.lastComparison = complexityAnalyzer.compareAlgorithms(
                    tabState.startId, tabState.endId, scenarioManager.isMobilityReduced());
                tabState.hasComparison = true;
                tabState.lastAction = "Complexity";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            yRight += px(48);

            if (tabState.hasComparison) {
                DrawText("Algoritmo", contentX, yRight, bodyFont, white);
                DrawText("Nodos", contentX + px(140), yRight, bodyFont, white);
                DrawText("Tiempo (us)", contentX + px(260), yRight, bodyFont, white);
                DrawText("Complejidad", contentX + px(420), yRight, bodyFont, white);
                yRight += px(24);
                DrawLine(contentX, yRight, contentX + contentW, yRight, Color{85, 98, 122, 255});
                yRight += px(10);

                DrawText("DFS", contentX, yRight, bodyFont, accentDfs);
                DrawText(TextFormat("%d", tabState.lastComparison.dfs_nodes_visited), contentX + px(140), yRight, bodyMutedFont, muted);
                DrawText(TextFormat("%lld", tabState.lastComparison.dfs_elapsed_us), contentX + px(260), yRight, bodyMutedFont, muted);
                DrawText("O(V+E)", contentX + px(420), yRight, bodyMutedFont, muted);
                yRight += px(22);

                DrawText("BFS", contentX, yRight, bodyFont, accentBfs);
                DrawText(TextFormat("%d", tabState.lastComparison.bfs_nodes_visited), contentX + px(140), yRight, bodyMutedFont, muted);
                DrawText(TextFormat("%lld", tabState.lastComparison.bfs_elapsed_us), contentX + px(260), yRight, bodyMutedFont, muted);
                DrawText("O(V+E)", contentX + px(420), yRight, bodyMutedFont, muted);
                yRight += px(32);

                std::string conclusion;
                Color conclusionColor = white;
                if (!tabState.lastComparison.dfs_reaches_destination &&
                    !tabState.lastComparison.bfs_reaches_destination) {
                    conclusion = "Ningun algoritmo alcanzo el destino.";
                    conclusionColor = bad;
                } else if (tabState.lastComparison.bfs_nodes_visited > tabState.lastComparison.dfs_nodes_visited) {
                    conclusion = "BFS visito mas nodos que DFS antes de detenerse.";
                    conclusionColor = accentBfs;
                } else if (tabState.lastComparison.bfs_nodes_visited < tabState.lastComparison.dfs_nodes_visited) {
                    conclusion = "DFS visito mas nodos que BFS antes de detenerse.";
                    conclusionColor = accentDfs;
                } else {
                    conclusion = "BFS y DFS visitaron la misma cantidad de nodos.";
                    conclusionColor = good;
                }
                DrawText(conclusion.c_str(), contentX, yRight, bodyFont, conclusionColor);
                yRight += px(28);
                DrawText(TextFormat("DFS alcanza destino: %s", tabState.lastComparison.dfs_reaches_destination ? "SI" : "NO"),
                         contentX, yRight, bodyMutedFont, tabState.lastComparison.dfs_reaches_destination ? good : bad);
                yRight += px(20);
                DrawText(TextFormat("BFS alcanza destino: %s", tabState.lastComparison.bfs_reaches_destination ? "SI" : "NO"),
                         contentX, yRight, bodyMutedFont, tabState.lastComparison.bfs_reaches_destination ? good : bad);
                yRight += px(20);
                DrawText(TextFormat("Diferencia de nodos visitados: %d",
                                    std::abs(tabState.lastComparison.bfs_nodes_visited -
                                             tabState.lastComparison.dfs_nodes_visited)),
                         contentX, yRight, bodyMutedFont, muted);
                yRight += px(20);
                if (tabState.lastComparison.bfs_elapsed_us > 0) {
                    const double ratio =
                        static_cast<double>(tabState.lastComparison.dfs_elapsed_us) /
                        static_cast<double>(tabState.lastComparison.bfs_elapsed_us);
                    DrawText(TextFormat("Ratio DFS/BFS: %.2fx", ratio), contentX, yRight, bodyMutedFont, muted);
                    yRight += px(20);
                }
                yRight += px(10);
                DrawText("Teoria: ambos algoritmos tienen O(V+E). BFS usa cola y DFS pila.", contentX, yRight, bodyMutedFont, muted);
            }
            break;
        }

        case 7: {
            DrawText("Puntos de Fallos y Bloqueos", contentX, yRight, sectionTitleFont, white);
            yRight += px(30);

            drawNodeSelector("Origen alterno:", tabState.startId, sizeof(tabState.startId),
                             state.startNodeDropdownOpen, state.startNodeDropdownScroll,
                             contentX, yRight, px(320));
            drawNodeSelector("Destino alterno:", tabState.endId, sizeof(tabState.endId),
                             state.endNodeDropdownOpen, state.endNodeDropdownScroll,
                             contentX, yRight, px(320));

            const auto& blockNodeOptions = runtimeBlockerService.nodeOptions();
            const auto& blockEdgeOptions = runtimeBlockerService.edgeOptions();

            DrawText("Nodo para bloquear:", contentX, yRight, bodyFont, white);
            yRight += px(22);
            if (!blockNodeOptions.empty()) {
                state.selectedBlockedNodeIdx =
                    std::clamp(state.selectedBlockedNodeIdx, 0, static_cast<int>(blockNodeOptions.size()) - 1);
                const std::string blockNodeLabel =
                    blockNodeOptions[state.selectedBlockedNodeIdx].label +
                    (state.blockedNodeDropdownOpen ? "  ^" : "  v");
                std::vector<std::string> blockNodeLabels;
                blockNodeLabels.reserve(blockNodeOptions.size());
                for (const auto& option : blockNodeOptions) blockNodeLabels.push_back(option.label);
                drawScrollableDropdown(blockNodeLabels, state.selectedBlockedNodeIdx,
                                       state.blockedNodeDropdownOpen, state.blockedNodeDropdownScroll,
                                       contentX, yRight, px(320), blockNodeLabel, bodyMutedFont);
            }

            DrawText("Conexion para bloquear:", contentX, yRight, bodyFont, white);
            yRight += px(22);
            if (!blockEdgeOptions.empty()) {
                state.selectedBlockedEdgeIdx =
                    std::clamp(state.selectedBlockedEdgeIdx, 0, static_cast<int>(blockEdgeOptions.size()) - 1);
                const std::string blockEdgeLabel =
                    blockEdgeOptions[state.selectedBlockedEdgeIdx].label +
                    (state.blockedEdgeDropdownOpen ? "  ^" : "  v");
                std::vector<std::string> blockEdgeLabels;
                blockEdgeLabels.reserve(blockEdgeOptions.size());
                for (const auto& option : blockEdgeOptions) blockEdgeLabels.push_back(option.label);
                drawScrollableDropdown(blockEdgeLabels, state.selectedBlockedEdgeIdx,
                                       state.blockedEdgeDropdownOpen, state.blockedEdgeDropdownScroll,
                                       contentX, yRight, contentW, blockEdgeLabel, bodyMutedFont);
            }

            Rectangle blockNodeBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                                   static_cast<float>(px(180)), static_cast<float>(buttonHeight)};
            Rectangle blockEdgeBtn{static_cast<float>(contentX + px(190)), static_cast<float>(yRight),
                                   static_cast<float>(px(200)), static_cast<float>(buttonHeight)};
            Rectangle clearBlocksBtn{static_cast<float>(contentX + px(400)), static_cast<float>(yRight),
                                     static_cast<float>(px(180)), static_cast<float>(buttonHeight)};
            if (!blockNodeOptions.empty() &&
                drawRayButton(blockNodeBtn, "Bloquear Nodo", bodyFont,
                              Color{180, 50, 50, 255}, Color{220, 70, 70, 255}, Color{255, 90, 90, 255}, white)) {
                runtimeBlockerService.blockNode(blockNodeOptions[state.selectedBlockedNodeIdx], resilienceService);
                tabState.lastAction = "BlockNode";
                routeState.routeRefreshCooldown = 0.0f;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            if (!blockEdgeOptions.empty() &&
                drawRayButton(blockEdgeBtn, "Bloquear Conexion", bodyFont,
                              Color{180, 50, 50, 255}, Color{220, 70, 70, 255}, Color{255, 90, 90, 255}, white)) {
                runtimeBlockerService.blockEdge(blockEdgeOptions[state.selectedBlockedEdgeIdx], resilienceService);
                tabState.lastAction = "BlockEdge";
                routeState.routeRefreshCooldown = 0.0f;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            if (drawRayButton(clearBlocksBtn, "Limpiar Bloqueos", bodyFont,
                              Color{50, 120, 50, 255}, Color{70, 150, 70, 255}, Color{90, 180, 90, 255}, white)) {
                runtimeBlockerService.clearAll(resilienceService);
                tabState.lastAction = "UnblockAll";
                routeState.routeRefreshCooldown = 0.0f;
                soundEffectService.play(SoundEffectType::SelectButton);
            }
            yRight += px(50);

            Rectangle altRouteBtn{static_cast<float>(contentX), static_cast<float>(yRight),
                                  static_cast<float>(px(260)), static_cast<float>(buttonHeight)};
            if (drawRayButton(altRouteBtn, "Buscar Ruta Alterna", bodyFont, btn, btnHover, btnActive, white)) {
                tabState.lastPath = runProfiledAlternatePath(graph, resilienceService, scenarioManager,
                                                             tabState.startId, tabState.endId);
                tabState.hasPath = true;
                tabState.lastAction = "AltPath";
                soundEffectService.play(SoundEffectType::RouteFixated);
            }
            yRight += px(46);

            DrawText(TextFormat("Conectividad global: %s", resilienceService.isStillConnected() ? "CONECTADO" : "FRAGMENTADO"),
                     contentX, yRight, bodyFont, resilienceService.isStillConnected() ? good : bad);
            yRight += px(24);
            DrawText(TextFormat("Bloqueos activos: %d nodos, %d conexiones",
                                static_cast<int>(blockedNodes.size()), static_cast<int>(blockedEdges.size())),
                     contentX, yRight, bodyFont, white);
            yRight += px(26);

            std::vector<std::string> blockLines;
            for (const auto& nodeId : blockedNodes) {
                blockLines.push_back("Nodo bloqueado: " + nodeId);
            }
            for (const auto& edge : blockedEdges) {
                blockLines.push_back("Conexion bloqueada: " + edge.first + " <-> " + edge.second);
            }
            if (blockLines.empty()) blockLines.push_back("No hay bloqueos activos.");
            drawPagedLines(blockLines, state.graphViewPage, contentX, yRight, contentW, px(140));

            if (tabState.lastAction == "AltPath" && tabState.hasPath) {
                DrawText(tabState.lastPath.found ? "RUTA ALTERNA ENCONTRADA" : "SIN RUTA ALTERNA DISPONIBLE",
                         contentX, yRight, bodyFont, tabState.lastPath.found ? good : bad);
                yRight += px(24);
                DrawText(TextFormat("Distancia alterna: %.2f", tabState.lastPath.total_weight),
                         contentX, yRight, bodyMutedFont, muted);
            }
            break;
        }

        default:
            break;
    }

    EndScissorMode();
    const int contentUsedHeight = std::max(0, yRight - scrollBaseY + px(24));
    const float maxScroll = static_cast<float>(std::max(0, contentUsedHeight - rightViewportHeight));
    state.analysisScroll = std::clamp(state.analysisScroll, 0.0f, maxScroll);
    if (maxScroll > 0.0f) {
        DrawText("Scroll con rueda para ver mas", contentX, rightViewportTop + rightViewportHeight - px(22),
                 bodyMutedFont, Color{185, 170, 235, 220});
    }

}

void UIManager::renderLegacyImGuiOverlay(State& state,
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
                                         const RouteRuntimeState& routeState) const {
    if (!state.showNavigationGraph) return;

    const auto nodeIds = graph.nodeIds();
    if (nodeIds.empty()) return;

    if (tabState.startId[0] == '\0') std::snprintf(tabState.startId, sizeof(tabState.startId), "%s", nodeIds.front().c_str());
    if (tabState.endId[0] == '\0') std::snprintf(tabState.endId, sizeof(tabState.endId), "%s", nodeIds.front().c_str());
    if (tabState.nodeId[0] == '\0') std::snprintf(tabState.nodeId, sizeof(tabState.nodeId), "%s", nodeIds.front().c_str());

    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::SetNextWindowPos(ImVec2(18.0f, 54.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(620.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Runtime Academico", &state.showNavigationGraph, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Visualizar Grafo de Navegacion");
    ImGui::SameLine();
    ImGui::TextDisabled("(overlay principal)");
    ImGui::Checkbox("Resaltar POIs (TAB)", &state.showInterestZones);

    int studentProfile = 1;
    if (scenarioManager.getStudentType() == StudentType::NEW_STUDENT) studentProfile = 0;
    if (scenarioManager.getStudentType() == StudentType::DISABLED_STUDENT) studentProfile = 2;

    bool mobilityReduced = scenarioManager.isMobilityReduced();
    if (studentProfile == 2) {
        ImGui::TextDisabled("Movilidad reducida forzada por perfil Discapacitado");
    } else if (ImGui::Checkbox("Escenario movilidad reducida", &mobilityReduced)) {
        scenarioManager.setMobilityReduced(mobilityReduced);
    }

    if (ImGui::RadioButton("Estudiante nuevo", studentProfile == 0)) {
        scenarioManager.setStudentType(StudentType::NEW_STUDENT);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Estudiante veterano", studentProfile == 1)) {
        scenarioManager.setStudentType(StudentType::VETERAN_STUDENT);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Discapacitado", studentProfile == 2)) {
        scenarioManager.setStudentType(StudentType::DISABLED_STUDENT);
    }

    comboSelectNode("Inicio manual DFS/BFS", nodeIds, tabState.startId, sizeof(tabState.startId));
    comboSelectNode("Destino manual", nodeIds, tabState.endId, sizeof(tabState.endId));
    comboSelectNode("Nodo para resiliencia", nodeIds, tabState.nodeId, sizeof(tabState.nodeId));

    if (ImGui::Button("Ejecutar DFS", ImVec2(120, 0))) {
        tabState.lastDfsTraversal = navService.runDfs(tabState.startId, scenarioManager.isMobilityReduced());
        tabState.lastTraversal = tabState.lastDfsTraversal;
        tabState.hasTraversal = true;
        tabState.hasDfsTraversal = true;
        tabState.lastAction = "DFS";
    }
    ImGui::SameLine();
    if (ImGui::Button("Ejecutar BFS", ImVec2(120, 0))) {
        tabState.lastBfsTraversal = navService.runBfs(tabState.startId, scenarioManager.isMobilityReduced());
        tabState.lastTraversal = tabState.lastBfsTraversal;
        tabState.hasTraversal = true;
        tabState.hasBfsTraversal = true;
        tabState.lastAction = "BFS";
    }
    ImGui::SameLine();
    if (ImGui::Button("Verificar Conexidad", ImVec2(170, 0))) {
        tabState.lastConnected = navService.checkConnectivity();
        tabState.hasConnectivityResult = true;
        tabState.lastAction = "Connectivity";
    }

    if (ImGui::Button("Buscar Camino DFS", ImVec2(170, 0))) {
        tabState.lastPath = runProfiledDfsPath(graph, navService, scenarioManager, tabState.startId, tabState.endId);
        tabState.hasPath = true;
        tabState.lastAction = "PathDFS";
    }
    ImGui::SameLine();
    if (ImGui::Button("Comparar BFS vs DFS", ImVec2(170, 0))) {
        tabState.lastStats = complexityAnalyzer.analyze(tabState.startId, scenarioManager.isMobilityReduced());
        tabState.lastComparison = complexityAnalyzer.compareAlgorithms(tabState.startId, tabState.endId,
                                                                       scenarioManager.isMobilityReduced());
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
                    ? "Nuevo"
                    : (scenarioManager.getStudentType() == StudentType::DISABLED_STUDENT ? "Discapacitado" : "Veterano"));
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
        ImGui::Text("Ultima ruta %s: %s", tabState.lastAction.c_str(),
                    tabState.lastPath.found ? "encontrada" : "sin ruta");
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
        Color baseNodeColor = nodeLevelColor(sceneId);
        if (isCurrentScene) baseNodeColor = lightenColor(baseNodeColor, 24);
        if (isBlockedScene) baseNodeColor = Color{220, 90, 90, 255};
        const ImU32 nodeColor = IM_COL32(baseNodeColor.r, baseNodeColor.g, baseNodeColor.b, baseNodeColor.a);
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
    const std::vector<std::string> highlightedPath = routeState.routeActive && !routeState.routeScenePlan.empty()
        ? routeState.routeScenePlan
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
