#include "WalkablePathService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>

Vector2 WalkablePathService::rectCenter(const Rectangle& rect) {
    return Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

float WalkablePathService::distanceBetween(const Vector2& a, const Vector2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

float WalkablePathService::polylineLength(const std::vector<Vector2>& points) {
    float total = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        total += distanceBetween(points[i - 1], points[i]);
    }
    return total;
}

bool WalkablePathService::intersectsAny(const Rectangle& rect,
                                        const std::vector<Rectangle>& obstacles) {
    for (const auto& obstacle : obstacles) {
        if (CheckCollisionRecs(rect, obstacle)) return true;
    }
    return false;
}

Rectangle WalkablePathService::playerColliderAt(const Vector2& playerPos) {
    Rectangle collider{};
    collider.x = playerPos.x - 5.0f;
    collider.y = playerPos.y - 8.0f;
    collider.width = 10.0f;
    collider.height = 8.0f;
    return collider;
}

std::vector<Vector2> WalkablePathService::buildWalkablePath(const MapRenderData& mapData,
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

Vector2 WalkablePathService::findSpawnPoint(const MapRenderData& mapData) {
    if (!mapData.hasTexture) return {100.0f, 100.0f};

    const float maxX = static_cast<float>(mapData.texture.width) - 20.0f;
    const float maxY = static_cast<float>(mapData.texture.height) - 20.0f;

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

    for (float y = maxY * 0.5f; y < maxY; y += 16.0f) {
        for (float x = 20.0f; x < maxX; x += 16.0f) {
            const Vector2 p{x, y};
            if (!intersectsAny(playerColliderAt(p), mapData.hitboxes)) return p;
        }
    }
    return {static_cast<float>(mapData.texture.width) * 0.5f, static_cast<float>(mapData.texture.height) * 0.5f};
}
