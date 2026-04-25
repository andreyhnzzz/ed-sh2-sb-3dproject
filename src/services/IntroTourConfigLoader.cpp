#include "IntroTourConfigLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace {
std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<Vector2> parsePointsArray(const json& pointsNode) {
    std::vector<Vector2> points;
    if (!pointsNode.is_array()) return points;

    for (const auto& entry : pointsNode) {
        if (!entry.is_array() || entry.size() != 2) continue;
        if (!entry[0].is_number() || !entry[1].is_number()) continue;

        points.push_back(Vector2{
            entry[0].get<float>(),
            entry[1].get<float>()
        });
    }
    return points;
}
} // namespace

std::string makeIntroTransitionKey(const std::string& fromScene, const std::string& toScene) {
    return toLowerCopy(fromScene) + "->" + toLowerCopy(toScene);
}

IntroTourConfig loadIntroTourConfig(const std::string& path) {
    IntroTourConfig config;
    if (path.empty()) {
        std::cerr << "[IntroTourConfig] Route file path is empty. Using defaults.\n";
        return config;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[IntroTourConfig] Could not open " << path << ". Using defaults.\n";
        return config;
    }

    json root;
    try {
        file >> root;
    } catch (const std::exception& ex) {
        std::cerr << "[IntroTourConfig] Parse error in " << path << ": " << ex.what() << "\n";
        return config;
    }

    if (root.contains("scene_order") && root["scene_order"].is_array()) {
        for (const auto& item : root["scene_order"]) {
            if (item.is_string()) config.sceneOrder.push_back(item.get<std::string>());
        }
    }

    if (root.contains("seconds_per_scene") && root["seconds_per_scene"].is_number()) {
        config.secondsPerScene = root["seconds_per_scene"].get<float>();
    }
    if (root.contains("transition_seconds") && root["transition_seconds"].is_number()) {
        config.transitionSeconds = root["transition_seconds"].get<float>();
    }
    if (root.contains("camera_zoom") && root["camera_zoom"].is_number()) {
        config.cameraZoom = root["camera_zoom"].get<float>();
    }
    if (root.contains("camera_follow_lerp") && root["camera_follow_lerp"].is_number()) {
        config.cameraFollowLerp = root["camera_follow_lerp"].get<float>();
    }

    if (root.contains("paths") && root["paths"].is_array()) {
        for (const auto& route : root["paths"]) {
            if (!route.is_object()) continue;

            const std::string from = route.value("from", "");
            const std::string to = route.value("to", "");
            if (from.empty() || to.empty()) continue;

            std::vector<Vector2> points = parsePointsArray(route.value("points", json::array()));
            if (points.size() < 2) continue;

            config.transitionPaths[makeIntroTransitionKey(from, to)] = std::move(points);
        }
    }

    if (root.contains("scene_fallbacks") && root["scene_fallbacks"].is_object()) {
        for (auto it = root["scene_fallbacks"].begin(); it != root["scene_fallbacks"].end(); ++it) {
            std::vector<Vector2> points = parsePointsArray(it.value());
            if (points.size() < 2) continue;
            config.sceneFallbackPaths[toLowerCopy(it.key())] = std::move(points);
        }
    }

    return config;
}
