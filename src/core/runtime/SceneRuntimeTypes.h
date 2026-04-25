#pragma once

#include <raylib.h>

#include <string>
#include <vector>

struct InterestZone {
    std::string name;
    std::vector<Rectangle> rects;
};

struct MapRenderData {
    Texture2D texture{};
    bool hasTexture{false};
    std::vector<Rectangle> hitboxes;
    std::vector<InterestZone> interestZones;
};

struct SceneData {
    Texture2D texture{};
    std::vector<Rectangle> hitboxes;
    std::vector<InterestZone> interestZones;
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
