#pragma once

#include "core/runtime/SceneRuntimeTypes.h"

#include <vector>

class MapRenderService {
public:
    static void drawMapWithHitboxes(const MapRenderData& mapData, bool showHitboxes);
    static void drawInterestZones(const std::vector<InterestZone>& zones);
};
