#pragma once
#include "VCRoad.h"
#include <string>
#include <vector>

struct DoorData {
    std::string id;
    glm::vec3   position;
    std::string target_room;
    bool        locked;
};

struct Room {
    std::string          id;
    std::string          name;
    VCRoadManager        vcRoad;
    std::vector<DoorData> doors;
    // world bounds (XZ)
    float min_x, max_x, min_z, max_z;
};
