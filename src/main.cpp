#include <raylib.h>
#include "rlImGui.h"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <limits>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <nlohmann/json.hpp>
#include "services/NavigationService.h"
#include "services/DataManager.h"
#include "services/ScenarioManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/ResilienceService.h"
#include "services/TransitionService.h"
#include "services/TmjLoader.h"
#include "ui/TabManager.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

struct MapRenderData {
    Texture2D texture{};
    bool hasTexture{false};
    std::vector<Rectangle> hitboxes;
};

struct SceneData {
    Texture2D texture{};
    std::vector<Rectangle> hitboxes;
    bool isValid{false};
};

struct SpriteAnim {
    Texture2D idle{};
    Texture2D walk{};
    bool hasIdle{false};
    bool hasWalk{false};
    int frameWidth{32};
    int frameHeight{32};
    int idleFrames{1};
    int walkFrames{1};
    int frame{0};
    float timer{0.0f};
    int direction{0}; // 0=down, 1=left, 2=right, 3=up
};

struct SceneConfig {
    std::string name;
    std::string pngPath;
    std::string tmjPath;
};

enum class SceneLinkType {
    Portal,
    Elevator,
    StairLeft,
    StairRight
};

struct SceneLink {
    std::string id;
    std::string fromScene;
    std::string toScene;
    std::string label;
    Rectangle triggerRect{};
    Vector2 arrivalSpawn{0.0f, 0.0f};
    SceneLinkType type{SceneLinkType::Portal};
};

static int directionStartFrame(int direction, int totalFrames) {
    if (totalFrames < 4) return 0;
    const int framesPerDir = std::max(1, totalFrames / 4);
    const int safeDir = std::clamp(direction, 0, 3);
    return safeDir * framesPerDir;
}

static int directionalFrameCount(int totalFrames) {
    if (totalFrames < 4) return std::max(1, totalFrames);
    return std::max(1, totalFrames / 4);
}

static Vector2 rectCenter(const Rectangle& rect) {
    return Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

static float distanceBetween(const Vector2& a, const Vector2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

static float polylineLength(const std::vector<Vector2>& points) {
    float total = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        total += distanceBetween(points[i - 1], points[i]);
    }
    return total;
}

static std::string formatElapsedTime(float elapsedSeconds) {
    const int totalSeconds = std::max(0, static_cast<int>(std::round(elapsedSeconds)));
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return TextFormat("%02d:%02d", minutes, seconds);
}

static std::vector<std::string> buildGraphOverviewLines(const CampusGraph& graph) {
    std::vector<std::string> lines;
    const auto ids = graph.nodeIds();
    lines.reserve(ids.size() * 3);
    lines.push_back(TextFormat("Nodes (%d):", static_cast<int>(ids.size())));
    for (const auto& id : ids) {
        lines.push_back(" - " + id);
    }

    lines.push_back("");
    lines.push_back("Connections (distance):");

    std::unordered_set<std::string> seenUndirected;
    int edgeIndex = 1;
    for (const auto& from : ids) {
        for (const auto& e : graph.edgesFrom(from)) {
            const std::string a = (e.from < e.to) ? e.from : e.to;
            const std::string b = (e.from < e.to) ? e.to : e.from;
            const std::string key = a + "|" + b + "|" + e.type;
            if (!seenUndirected.insert(key).second) continue;

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss.precision(2);
            oss << edgeIndex++ << ". " << a << " <-> " << b
                << " | base=" << e.base_weight
                << " | mr=" << e.mobility_weight;
            lines.push_back(oss.str());
        }
    }

    return lines;
}

static std::vector<std::string> buildTraversalDetailLines(const TraversalResult& traversal,
                                                          const char* title) {
    std::vector<std::string> lines;
    lines.reserve(traversal.visit_order.size() + 3);
    lines.push_back(TextFormat("%s (visited=%d):", title, traversal.nodes_visited));
    if (traversal.visit_order.empty()) {
        lines.push_back("No traversal data.");
        return lines;
    }

    for (size_t i = 0; i < traversal.visit_order.size(); ++i) {
        const auto& node = traversal.visit_order[i];
        const auto it = traversal.accumulated_dist.find(node);
        const double acc = (it != traversal.accumulated_dist.end()) ? it->second : 0.0;

        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << (i + 1) << ". " << node << " | acc=" << acc;
        lines.push_back(oss.str());
    }

    return lines;
}

static bool isLinkAllowed(const SceneLink& link, bool mobilityReduced) {
    if (!mobilityReduced) return true;
    return link.type != SceneLinkType::StairLeft &&
           link.type != SceneLinkType::StairRight;
}

static std::vector<std::string>
buildScenePlan(const std::string& startScene,
               const std::string& goalScene,
               const std::vector<SceneLink>& links,
               bool mobilityReduced) {
    if (startScene.empty() || goalScene.empty()) return {};
    if (startScene == goalScene) return {startScene};

    std::queue<std::string> pending;
    std::unordered_map<std::string, std::string> previous;
    std::unordered_set<std::string> visited;

    pending.push(startScene);
    visited.insert(startScene);

    while (!pending.empty()) {
        const std::string scene = pending.front();
        pending.pop();

        for (const auto& link : links) {
            if (link.fromScene != scene || !isLinkAllowed(link, mobilityReduced)) continue;
            if (!visited.insert(link.toScene).second) continue;
            previous[link.toScene] = scene;

            if (link.toScene == goalScene) {
                std::vector<std::string> plan;
                std::string current = goalScene;
                plan.push_back(current);
                while (current != startScene) {
                    const auto it = previous.find(current);
                    if (it == previous.end()) return {};
                    current = it->second;
                    plan.push_back(current);
                }
                std::reverse(plan.begin(), plan.end());
                return plan;
            }

            pending.push(link.toScene);
        }
    }

    return {};
}

static std::string findPathCandidate(const char* argv0, const std::vector<fs::path>& baseCandidates) {
    std::vector<fs::path> candidates = baseCandidates;

    if (argv0 && argv0[0] != '\0') {
        const fs::path exePath = fs::absolute(argv0).parent_path();
        for (const auto& base : baseCandidates) {
            candidates.emplace_back(exePath / base);
            candidates.emplace_back(exePath / ".." / base);
            candidates.emplace_back(exePath / "../.." / base);
        }
    }

    for (const auto& c : candidates) {
        if (fs::exists(c)) return c.string();
    }
    return "";
}

static std::string findCampusJson(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("campus.json"),
        fs::path("../campus.json"),
        fs::path("../../campus.json"),
        fs::path("../EcoCampusNav/campus.json")
    });
}

static std::string resolveAssetPath(const char* argv0, const std::string& relPath) {
    return findPathCandidate(argv0, {
        fs::path(relPath),
        fs::path("..") / relPath,
        fs::path("../..") / relPath
    });
}

static std::string findPlayerIdleSprite(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("assets/sprites/m_Character/junior_AnguloIdle.png"),
        fs::path("../assets/sprites/m_Character/junior_AnguloIdle.png"),
        fs::path("../../assets/sprites/m_Character/junior_AnguloIdle.png")
    });
}

static std::string findPlayerWalkSprite(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("assets/sprites/m_Character/junior_AnguloWalk.png"),
        fs::path("../assets/sprites/m_Character/junior_AnguloWalk.png"),
        fs::path("../../assets/sprites/m_Character/junior_AnguloWalk.png")
    });
}

static bool intersectsAny(const Rectangle& rect, const std::vector<Rectangle>& obstacles) {
    for (const auto& obstacle : obstacles) {
        if (CheckCollisionRecs(rect, obstacle)) return true;
    }
    return false;
}

static Rectangle playerColliderAt(const Vector2& playerPos) {
    Rectangle collider{};
    // Huella de colision en los pies para movimiento top-down.
    collider.x = playerPos.x - 5.0f;
    collider.y = playerPos.y - 8.0f;
    collider.width = 10.0f;
    collider.height = 8.0f;
    return collider;
}

static std::vector<Vector2> buildWalkablePath(const MapRenderData& mapData,
                                              const Vector2& start,
                                              const Vector2& goal) {
    if (!mapData.hasTexture) return {};

    constexpr float kCellSize = 16.0f;
    const float texW = static_cast<float>(mapData.texture.width);
    const float texH = static_cast<float>(mapData.texture.height);
    const int cols = std::max(1, static_cast<int>(std::ceil(texW / kCellSize)));
    const int rows = std::max(1, static_cast<int>(std::ceil(texH / kCellSize)));
    const int cellCount = cols * rows;

    auto idxOf = [cols](int x, int y) { return y * cols + x; };
    auto cellX = [cols](int idx) { return idx % cols; };
    auto cellY = [cols](int idx) { return idx / cols; };
    auto clampCell = [&](int& x, int& y) {
        x = std::clamp(x, 0, cols - 1);
        y = std::clamp(y, 0, rows - 1);
    };
    auto cellCenter = [&](int x, int y) {
        return Vector2{
            std::clamp((static_cast<float>(x) + 0.5f) * kCellSize, 8.0f, texW - 8.0f),
            std::clamp((static_cast<float>(y) + 0.5f) * kCellSize, 14.0f, texH - 8.0f)
        };
    };

    std::vector<int8_t> walkable(cellCount, -1);
    auto isWalkableCell = [&](int x, int y) {
        clampCell(x, y);
        const int idx = idxOf(x, y);
        if (walkable[idx] != -1) return walkable[idx] == 1;
        const bool freeCell = !intersectsAny(playerColliderAt(cellCenter(x, y)), mapData.hitboxes);
        walkable[idx] = freeCell ? 1 : 0;
        return freeCell;
    };

    auto nearestWalkableCell = [&](const Vector2& point) {
        int baseX = static_cast<int>(point.x / kCellSize);
        int baseY = static_cast<int>(point.y / kCellSize);
        clampCell(baseX, baseY);
        if (isWalkableCell(baseX, baseY)) return idxOf(baseX, baseY);

        const int maxRadius = std::max(cols, rows);
        int bestIdx = -1;
        float bestDistance = std::numeric_limits<float>::max();
        for (int radius = 1; radius <= maxRadius; ++radius) {
            const int minX = std::max(0, baseX - radius);
            const int maxX = std::min(cols - 1, baseX + radius);
            const int minY = std::max(0, baseY - radius);
            const int maxY = std::min(rows - 1, baseY + radius);

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    const bool edgeCell = (x == minX || x == maxX || y == minY || y == maxY);
                    if (!edgeCell || !isWalkableCell(x, y)) continue;
                    const float dist = distanceBetween(point, cellCenter(x, y));
                    if (dist < bestDistance) {
                        bestDistance = dist;
                        bestIdx = idxOf(x, y);
                    }
                }
            }
            if (bestIdx >= 0) return bestIdx;
        }

        return -1;
    };

    const int startIdx = nearestWalkableCell(start);
    const int goalIdx = nearestWalkableCell(goal);
    if (startIdx < 0 || goalIdx < 0) return {};
    if (startIdx == goalIdx) return {start, goal};

    struct OpenNode {
        float fScore;
        int idx;
    };
    struct OpenNodeCompare {
        bool operator()(const OpenNode& a, const OpenNode& b) const {
            return a.fScore > b.fScore;
        }
    };

    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeCompare> open;
    std::vector<float> gScore(cellCount, std::numeric_limits<float>::max());
    std::vector<int> parent(cellCount, -1);
    std::vector<bool> closed(cellCount, false);

    gScore[startIdx] = 0.0f;
    open.push({distanceBetween(cellCenter(cellX(startIdx), cellY(startIdx)),
                               cellCenter(cellX(goalIdx), cellY(goalIdx))),
               startIdx});

    while (!open.empty()) {
        const int current = open.top().idx;
        open.pop();
        if (closed[current]) continue;
        if (current == goalIdx) break;
        closed[current] = true;

        const int cx = cellX(current);
        const int cy = cellY(current);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const int nx = cx + dx;
                const int ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= cols || ny >= rows) continue;
                if (!isWalkableCell(nx, ny)) continue;
                if (dx != 0 && dy != 0 &&
                    (!isWalkableCell(cx + dx, cy) || !isWalkableCell(cx, cy + dy))) {
                    continue;
                }

                const int next = idxOf(nx, ny);
                const float stepCost = (dx != 0 && dy != 0) ? 1.41421356f : 1.0f;
                const float tentative = gScore[current] + stepCost;
                if (tentative >= gScore[next]) continue;

                parent[next] = current;
                gScore[next] = tentative;
                const float heuristic =
                    distanceBetween(cellCenter(nx, ny),
                                    cellCenter(cellX(goalIdx), cellY(goalIdx)));
                open.push({tentative + heuristic, next});
            }
        }
    }

    if (parent[goalIdx] == -1) return {start, goal};

    std::vector<Vector2> path;
    for (int current = goalIdx; current != -1; current = parent[current]) {
        path.push_back(cellCenter(cellX(current), cellY(current)));
        if (current == startIdx) break;
    }
    std::reverse(path.begin(), path.end());
    if (!path.empty()) {
        path.front() = start;
        path.back() = goal;
    }
    return path;
}

static Vector2 findSpawnPoint(const MapRenderData& mapData) {
    if (!mapData.hasTexture) return {100.0f, 100.0f};

    const float maxX = static_cast<float>(mapData.texture.width) - 20.0f;
    const float maxY = static_cast<float>(mapData.texture.height) - 20.0f;

    // Priorizar spawn en esquina inferior derecha (zona parada de bus).
    const float preferredX = std::clamp(maxX * 0.88f, 20.0f, maxX);
    const float preferredY = std::clamp(maxY * 0.90f, 40.0f, maxY);
    for (float radius = 0.0f; radius <= 240.0f; radius += 16.0f) {
        for (float dy = -radius; dy <= radius; dy += 16.0f) {
            for (float dx = -radius; dx <= radius; dx += 16.0f) {
                const float x = std::clamp(preferredX + dx, 20.0f, maxX);
                const float y = std::clamp(preferredY + dy, 40.0f, maxY);
                const Vector2 p{x, y};
                if (!intersectsAny(playerColliderAt(p), mapData.hitboxes)) return p;
            }
        }
    }

    // Fallback general.
    for (float y = maxY * 0.5f; y < maxY; y += 16.0f) {
        for (float x = 20.0f; x < maxX; x += 16.0f) {
            const Vector2 p{x, y};
            if (!intersectsAny(playerColliderAt(p), mapData.hitboxes)) return p;
        }
    }
    return {static_cast<float>(mapData.texture.width) * 0.5f, static_cast<float>(mapData.texture.height) * 0.5f};
}

static void drawMapWithHitboxes(const MapRenderData& mapData, bool showHitboxes) {
    if (!mapData.hasTexture) return;
    DrawTexture(mapData.texture, 0, 0, WHITE);

    if (!showHitboxes) return;

    for (const auto& h : mapData.hitboxes) {
        DrawRectangleLinesEx(h, 1.5f, Color{255, 80, 80, 220});
    }
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
    const TabManagerState& state,
    const std::string& currentSceneName,
    bool showHitboxes,
    bool showTriggers,
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
    DrawText(TextFormat("Reduced mobility: %s", mobilityReduced ? "ON" : "OFF"), rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(22);
    DrawText(TextFormat("Student profile: %s", studentType == StudentType::NEW_STUDENT ? "New" : "Regular"),
             rightX + sectionPad, yRight, bodyMutedFont, muted); yRight += px(26);

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
    const std::string elapsedLabel = formatElapsedTime(routeTravelElapsed);
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
        activeLines = buildGraphOverviewLines(graph);
        activePage = &graphPage;
    } else if (rubricViewMode == 1) {
        activeLines = buildTraversalDetailLines(dfsTraversal, "DFS order + accumulated distance");
        activePage = &dfsPage;
    } else {
        activeLines = buildTraversalDetailLines(bfsTraversal, "BFS order + accumulated distance");
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

    std::string path = findCampusJson(argc > 0 ? argv[0] : nullptr);
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
        const std::string tmjPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (!tmjPath.empty()) {
            sceneToTmjPath[sc.name] = tmjPath;
        }
    }

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
        const std::string tmjPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
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
        sceneDataMap[sc.name] = std::move(sd);
    }

    // -----------------------------------------------------------------------
    // Transitions: load portals from TMJ, resolve spawn positions, register
    // -----------------------------------------------------------------------
    TransitionService transitions;

    for (const auto& sc : allScenes) {
        const std::string tmjPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
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
    {
        const auto& initConfig = sceneMap.at(initialSceneName);
        const std::string pngPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.pngPath);
        if (!pngPath.empty()) {
            mapData.texture = LoadTexture(pngPath.c_str());
            mapData.hasTexture = mapData.texture.id != 0;
        } else {
            std::cerr << "Could not find " << initConfig.pngPath << "\n";
        }
        const auto sdIt = sceneDataMap.find(initialSceneName);
        if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
            mapData.hitboxes = sdIt->second.hitboxes;
        }
    }

    SpriteAnim playerAnim;
    const std::string idlePath = findPlayerIdleSprite(argc > 0 ? argv[0] : nullptr);
    const std::string walkPath = findPlayerWalkSprite(argc > 0 ? argv[0] : nullptr);
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
        return findSpawnPoint(mapData);
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
        if (!intersectsAny(playerColliderAt(candidate), mapData.hitboxes)) {
            playerPos.x = candidate.x;
        }

        candidate = playerPos;
        candidate.y += moveY * currentSpeed * dt;
        if (mapData.hasTexture) {
            candidate.y = std::clamp(candidate.y, 14.0f, static_cast<float>(mapData.texture.height));
        }
        if (!intersectsAny(playerColliderAt(candidate), mapData.hitboxes)) {
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
            const bool movedEnough = distanceBetween(playerPos, routeAnchorPos) >= 28.0f;
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
                    const float currentToGoal = distanceBetween(playerPos, goal);
                    if (routeLegSceneId != currentSceneId || routeLegNextSceneId != routeTargetScene ||
                        routeLegStartDistance <= 0.0f) {
                        routeLegSceneId = currentSceneId;
                        routeLegNextSceneId = routeTargetScene;
                        routeLegStartDistance = std::max(currentToGoal, 1.0f);
                    }

                    const float localProgress = std::clamp(1.0f - (currentToGoal / routeLegStartDistance), 0.0f, 1.0f);
                    routeProgressPct = std::max(routeProgressPct, localProgress * 100.0f);
                    routePathPoints = buildWalkablePath(mapData, playerPos, goal);
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
                            !isLinkAllowed(link, routeMobilityReduced)) {
                            continue;
                        }
                        const std::vector<Vector2> candidatePath =
                            buildWalkablePath(mapData, playerPos, rectCenter(link.triggerRect));
                        if (candidatePath.empty()) continue;
                        const float len = polylineLength(candidatePath);
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
                        const float currentToGoal = distanceBetween(playerPos, routePathPoints.back());
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

        traversalRefreshCooldown -= dt;
        if (infoMenuOpen && traversalRefreshCooldown <= 0.0f) {
            const std::string traversalStart = canonicalSceneId(currentSceneName);
            dfsTraversalView = nav_service.runDfs(traversalStart, scenario_manager.isMobilityReduced());
            bfsTraversalView = nav_service.runBfs(traversalStart, scenario_manager.isMobilityReduced());
            traversalRefreshCooldown = 0.2f;
        }

        // --- Scene transition (portal & elevator detection + fade state machine) ---
        if (!infoMenuOpen) {
            transitions.update(playerColliderAt(playerPos), currentSceneName, dt);
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

            const auto scIt = sceneMap.find(req.targetScene);
            if (scIt != sceneMap.end()) {
                const std::string pngPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, scIt->second.pngPath);
                if (!pngPath.empty()) {
                    mapData.texture    = LoadTexture(pngPath.c_str());
                    mapData.hasTexture = mapData.texture.id != 0;
                }
                const auto sdIt = sceneDataMap.find(req.targetScene);
                if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
                    mapData.hitboxes = sdIt->second.hitboxes;
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
                playerAnim.frame = (playerAnim.frame + 1) % directionalFrameCount(playerAnim.walkFrames);
            }
        } else {
            playerAnim.timer = 0.0f;
            playerAnim.frame = 0;
        }

        BeginDrawing();
        ClearBackground({18, 20, 28, 255});
        BeginMode2D(camera);
        if (mapData.hasTexture) {
            drawMapWithHitboxes(mapData, showHitboxes);
        } else {
            DrawRectangle(0, 0, screenWidth, screenHeight, {22, 26, 36, 255});
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
            const int baseFrame = directionStartFrame(playerAnim.direction, activeFrames);
            const int frameCount = directionalFrameCount(activeFrames);
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
            tabState,
            currentSceneName,
            showHitboxes,
            showTriggers,
            scenario_manager.isMobilityReduced(),
            scenario_manager.getStudentType(),
            resilience_service.getBlockedNodes(),
            resilience_service.isStillConnected());

        rlImGuiBegin();

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
