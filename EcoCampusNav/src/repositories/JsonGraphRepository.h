#pragma once
#include "../core/graph/CampusGraph.h"
#include <string>
#include <stdexcept>

class JsonGraphRepository {
public:
    static CampusGraph loadFromFile(const std::string& path);
};
