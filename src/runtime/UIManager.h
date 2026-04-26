#pragma once

#include "InputState.h"
#include "RenderContext.h"
#include "runtime/RuntimeNavigationManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/DestinationCatalog.h"
#include "services/NavigationService.h"
#include "services/MusicService.h"
#include "services/ResilienceService.h"
#include "services/RuntimeBlockerService.h"
#include "services/ScenarioManager.h"
#include "services/SoundEffectService.h"
#include "services/TransitionService.h"
#include "ui/TabManager.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class UIManager {
public:
    struct State {
        bool showNavigationGraph{false};
        bool showHitboxes{false};
        bool showTriggers{false};
        bool showInterestZones{true};
        bool infoMenuOpen{false};
        int activeMenuTab{0};
        TraversalResult dfsTraversalView;
        TraversalResult bfsTraversalView;
        int rubricViewMode{0};
        int graphViewPage{0};
        int dfsViewPage{0};
        int bfsViewPage{0};
        int selectedBlockedNodeIdx{0};
        int selectedBlockedEdgeIdx{0};
        float traversalRefreshCooldown{0.0f};
    };

    void handleInput(const InputState& input, State& state) const;
    void refreshTraversalViews(State& state,
                               const std::string& currentSceneId,
                               NavigationService& navService,
                               bool mobilityReduced) const;

    void renderWorld(const RenderContext& ctx,
                     const CampusGraph& graph,
                     const std::vector<SceneLink>& sceneLinks,
                     const DestinationCatalog& destinationCatalog,
                     const std::vector<std::string>& blockedNodes,
                     bool mobilityReduced,
                     const TransitionService& transitions) const;

    void renderScreen(const RenderContext& ctx,
                      State& state,
                      RouteRuntimeState& routeState,
                      const std::vector<std::pair<std::string, std::string>>& routeScenes,
                      const std::function<std::string(const std::string&)>& sceneDisplayName,
                      const CampusGraph& graph,
                      TabManagerState& tabState,
                      NavigationService& navService,
                      ScenarioManager& scenarioManager,
                      ComplexityAnalyzer& complexityAnalyzer,
                      RuntimeBlockerService& runtimeBlockerService,
                      const DestinationCatalog& destinationCatalog,
                      MusicService& musicService,
                      SoundEffectService& soundEffectService,
                      ResilienceService& resilienceService,
                      TransitionService& transitions,
                      const std::unordered_map<std::string, SceneData>& sceneDataMap) const;

private:
    void renderMinimap(const RenderContext& ctx, const std::vector<Rectangle>& blockedRects) const;
    void renderCoordinateDisplay(const Vector2& playerPos, int screenWidth) const;
    void renderPrompt(const TransitionService& transitions, int screenWidth, int screenHeight) const;
    void renderNavigationOverlayMenu(bool showNavigationGraph,
                                     bool infoMenuOpen,
                                     const std::string& currentSceneName,
                                     bool mobilityReduced,
                                     StudentType studentType,
                                     bool routeActive,
                                     float routeProgressPct,
                                     bool resilienceConnected,
                                     const TabManagerState& state,
                                     const std::vector<std::string>& blockedNodes) const;

    void renderInfoMenu(const RenderContext& ctx,
                        State& state,
                        RouteRuntimeState& routeState,
                        const std::vector<std::pair<std::string, std::string>>& routeScenes,
                        const std::function<std::string(const std::string&)>& sceneDisplayName,
                        const CampusGraph& graph,
                        TabManagerState& tabState,
                        NavigationService& navService,
                        ScenarioManager& scenarioManager,
                        ComplexityAnalyzer& complexityAnalyzer,
                        RuntimeBlockerService& runtimeBlockerService,
                        const DestinationCatalog& destinationCatalog,
                        MusicService& musicService,
                        SoundEffectService& soundEffectService,
                        ResilienceService& resilienceService) const;

    void renderLegacyImGuiOverlay(State& state,
                                  TabManagerState& tabState,
                                  NavigationService& navService,
                                  ScenarioManager& scenarioManager,
                                  ComplexityAnalyzer& complexityAnalyzer,
                                  ResilienceService& resilienceService,
                                  const CampusGraph& graph,
                                  const std::unordered_map<std::string, SceneData>& sceneDataMap,
                                  const std::vector<std::pair<std::string, std::string>>& routeScenes,
                                  const std::function<std::string(const std::string&)>& sceneDisplayName,
                                  const std::string& currentSceneName,
                                  const RouteRuntimeState& routeState) const;
};
