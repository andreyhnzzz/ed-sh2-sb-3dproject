#pragma once

#include "core/runtime/SceneRuntimeTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

class InterestZoneLoader {
public:
    using ZonesByScene = std::unordered_map<std::string, std::vector<InterestZone>>;

    static ZonesByScene loadFromJson(const std::string& jsonPath);
};
