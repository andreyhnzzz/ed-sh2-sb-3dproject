#include "MapRenderService.h"

#include <raylib.h>

void MapRenderService::drawMapWithHitboxes(const MapRenderData& mapData, bool showHitboxes) {
    if (!mapData.hasTexture) return;
    DrawTexture(mapData.texture, 0, 0, WHITE);

    if (!showHitboxes) return;

    for (const auto& h : mapData.hitboxes) {
        DrawRectangleLinesEx(h, 1.5f, Color{255, 80, 80, 220});
    }
}

void MapRenderService::drawInterestZones(const std::vector<InterestZone>& zones) {
    const Color fill = Color{255, 200, 60, 60};
    const Color border = Color{255, 180, 40, 200};
    const Color labelBg = Color{20, 20, 20, 200};
    const Color labelFg = Color{255, 230, 160, 230};
    const int fontSize = 14;

    for (const auto& zone : zones) {
        for (const auto& r : zone.rects) {
            DrawRectangleRec(r, fill);
            DrawRectangleLinesEx(r, 2.0f, border);
        }
        if (!zone.rects.empty() && !zone.name.empty()) {
            const auto& r0 = zone.rects.front();
            const int textW = MeasureText(zone.name.c_str(), fontSize);
            const int textX = static_cast<int>(r0.x) + 6;
            const int textY = static_cast<int>(r0.y) + 6;
            DrawRectangle(textX - 4, textY - 3, textW + 8, fontSize + 6, labelBg);
            DrawText(zone.name.c_str(), textX, textY, fontSize, labelFg);
        }

        Vector2 marker = {0.0f, 0.0f};
        for (const auto& r : zone.rects) {
            marker.x += r.x + r.width * 0.5f;
            marker.y += r.y + r.height * 0.5f;
        }
        if (!zone.rects.empty()) {
            marker.x /= static_cast<float>(zone.rects.size());
            marker.y /= static_cast<float>(zone.rects.size());
            DrawCircleV(marker, 7.0f, Color{255, 190, 40, 235});
            DrawCircleLines(static_cast<int>(marker.x), static_cast<int>(marker.y), 7.0f,
                            Color{255, 245, 180, 240});
        }
    }
}
