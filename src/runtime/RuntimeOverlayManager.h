#pragma once
// MIGRADO DESDE main.cpp: Líneas ~267-355, 636-708 (drawCurrentSceneNavigationOverlay, drawRaylibNavigationOverlayMenu)
// Responsabilidad: Renderizar overlays de navegación en escena actual y menú de estado

#include "../core/graph/CampusGraph.h"
#include "../core/runtime/SceneRuntimeTypes.h"
#include "../services/DestinationCatalog.h"
#include "../ui/TabManager.h"
#include <vector>
#include <string>
#include <functional>

class RuntimeOverlayManager {
public:
    // MIGRADO DESDE main.cpp:267-355
    static void drawCurrentSceneNavigationOverlay(
        const CampusGraph& graph,
        const std::string& currentSceneName,
        const std::vector<SceneLink>& sceneLinks,
        const DestinationCatalog& destinationCatalog,
        const std::vector<std::string>& activePathNodes,
        const std::vector<std::string>& blockedNodes,
        bool mobilityReduced);

    // MIGRADO DESDE main.cpp:636-708
    static void drawRaylibNavigationOverlayMenu(
        bool showNavigationGraph,
        bool infoMenuOpen,
        const std::string& currentSceneName,
        bool mobilityReduced,
        StudentType studentType,
        bool routeActive,
        float routeProgressPct,
        bool resilienceConnected,
        const TabManagerState& state,
        const std::vector<std::string>& blockedNodes);
};
