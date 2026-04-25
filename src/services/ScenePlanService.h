#pragma once

#include "core/runtime/SceneRuntimeTypes.h"

#include <raylib.h>

#include <string>
#include <vector>

class ScenePlanService {
public:
    static bool isLinkAllowed(const SceneLink& link, bool mobilityReduced);

    static std::vector<std::string> buildScenePlan(
        const std::string& startScene,
        const std::string& goalScene,
        const std::vector<SceneLink>& links,
        bool mobilityReduced);
};
