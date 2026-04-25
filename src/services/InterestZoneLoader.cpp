#include "InterestZoneLoader.h"

#include "services/StringUtils.h"

#include <fstream>
#include <iostream>
#include <utility>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

InterestZoneLoader::ZonesByScene InterestZoneLoader::loadFromJson(const std::string& jsonPath) {
    ZonesByScene zonesByScene;
    if (jsonPath.empty()) return zonesByScene;

    std::ifstream input(jsonPath);
    if (!input.is_open()) {
        std::cerr << "[InterestZones] Cannot open: " << jsonPath << "\n";
        return zonesByScene;
    }

    json data;
    try {
        input >> data;
    } catch (const std::exception& ex) {
        std::cerr << "[InterestZones] Parse error: " << ex.what() << "\n";
        return zonesByScene;
    }

    if (!data.contains("scenes") || !data["scenes"].is_object()) return zonesByScene;

    for (auto it = data["scenes"].begin(); it != data["scenes"].end(); ++it) {
        const std::string sceneId = StringUtils::toLowerCopy(it.key());
        if (!it.value().is_array()) continue;

        for (const auto& zoneJson : it.value()) {
            if (!zoneJson.contains("name") || !zoneJson.contains("rects")) continue;

            InterestZone zone;
            zone.name = zoneJson.value("name", "");
            if (!zoneJson["rects"].is_array()) continue;

            for (const auto& rj : zoneJson["rects"]) {
                if (!rj.contains("x") || !rj.contains("y") ||
                    !rj.contains("w") || !rj.contains("h")) {
                    continue;
                }
                Rectangle r{};
                r.x = rj["x"].get<float>();
                r.y = rj["y"].get<float>();
                r.width = rj["w"].get<float>();
                r.height = rj["h"].get<float>();
                zone.rects.push_back(r);
            }

            if (!zone.rects.empty()) zonesByScene[sceneId].push_back(std::move(zone));
        }
    }

    return zonesByScene;
}
