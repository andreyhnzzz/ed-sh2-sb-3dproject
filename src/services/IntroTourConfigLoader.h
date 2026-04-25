#pragma once

#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

struct IntroTourConfig {
    std::vector<std::string> sceneOrder;
    std::unordered_map<std::string, std::vector<Vector2>> transitionPaths;
    std::unordered_map<std::string, std::vector<Vector2>> sceneFallbackPaths;
    float secondsPerScene{15.0f};
    float transitionSeconds{2.2f};
    float cameraZoom{2.25f};
    float cameraFollowLerp{2.6f};
};

std::string makeIntroTransitionKey(const std::string& fromScene, const std::string& toScene);
IntroTourConfig loadIntroTourConfig(const std::string& path);
