#pragma once

#include "core/application/WindowInitializer.h"
#include "runtime/GameController.h"
#include "runtime/GameplayLoopController.h"
#include "runtime/InputManager.h"
#include "runtime/RuntimeNavigationManager.h"
#include "runtime/SceneManager.h"
#include "runtime/StartMenuController.h"
#include "runtime/UIManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/DataManager.h"
#include "services/DestinationCatalog.h"
#include "services/MusicService.h"
#include "services/NavigationService.h"
#include "services/ResilienceService.h"
#include "services/RuntimeBlockerService.h"
#include "services/ScenarioManager.h"
#include "services/SoundEffectService.h"
#include "services/TransitionService.h"
#include "services/initialization/IntroTourSetup.h"
#include "services/initialization/SceneBootstrap.h"
#include "ui/TabManager.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

class ApplicationSession {
public:
    ApplicationSession() = default;
    ~ApplicationSession();

    bool initialize(int argc, char* argv[]);
    int run();

private:
    static constexpr double kPixelsToMeters = 0.10;
    static constexpr bool kIntroMenuEnabled = true;

    bool initializeRuntime();
    bool loadGraphData();
    bool initializeNavigationDomain();
    void initializeControllers();
    void shutdown();
    std::string canonicalSceneId(std::string sceneName) const;
    Vector2 resolveGameplaySpawnPos() const;

    bool initialized_{false};
    const char* executablePath_{nullptr};
    std::string campusJsonPath_;
    WindowConfig windowConfig_{};

    MusicService musicService_{};
    SoundEffectService soundEffectService_{};
    DataManager dataManager_{};
    SceneBootstrap sceneBootstrap_{};
    CampusGraph graph_{};
    std::optional<NavigationService> navigationService_;
    ScenarioManager scenarioManager_{};
    std::optional<ComplexityAnalyzer> complexityAnalyzer_;
    std::optional<ResilienceService> resilienceService_;
    DestinationCatalog destinationCatalog_{};
    std::vector<SceneLink> sceneLinks_{};
    RuntimeBlockerService runtimeBlockerService_{};
    TransitionService transitions_{};
    std::vector<std::pair<std::string, std::string>> routeScenes_{};

    InputManager inputManager_{};
    GameController gameController_{};
    SceneManager sceneManager_{};
    UIManager uiManager_{};
    std::optional<RuntimeNavigationManager> runtimeNavigation_;
    TabManagerState tabState_{};
    RouteRuntimeState routeState_{};
    UIManager::State uiState_{};
    IntroTourResources introResources_{};

    std::optional<GameplayLoopController> gameplayController_;
    std::optional<StartMenuController> startMenuController_;
};
