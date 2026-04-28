#include "core/application/ApplicationSession.h"

#include "core/application/AudioInitializer.h"
#include "services/AssetPathResolver.h"
#include "services/AudioManager.h"
#include "services/WalkablePathService.h"
#include "services/initialization/FloorLinkLoader.h"

#include <algorithm>
#include <cctype>
#include <iostream>

ApplicationSession::~ApplicationSession() {
    shutdown();
}

bool ApplicationSession::initialize(int argc, char* argv[]) {
    executablePath_ = argc > 0 ? argv[0] : nullptr;
    campusJsonPath_ = AssetPathResolver::findCampusJson(executablePath_);
    if (campusJsonPath_.empty()) {
        std::cerr << "campus.json was not found. Place the file in the working directory.\n";
        return false;
    }

    if (!initializeRuntime()) {
        return false;
    }

    if (!loadGraphData()) {
        shutdown();
        return false;
    }

    if (!initializeNavigationDomain()) {
        shutdown();
        return false;
    }

    initializeControllers();
    initialized_ = true;
    return true;
}

int ApplicationSession::run() {
    if (!initialized_) {
        return 1;
    }

    if (!kIntroMenuEnabled && gameplayController_.has_value()) {
        gameplayController_->begin(introResources_.gameplayInitialSceneName, resolveGameplaySpawnPos());
    }

    bool exitRequested = false;
    while (!WindowShouldClose() && !exitRequested) {
        const float dt = GetFrameTime();
        musicService_.update();

        if (startMenuController_.has_value() && startMenuController_->isActive()) {
            const StartMenuAction action =
                startMenuController_->updateAndRender(dt, musicService_, soundEffectService_);
            if (action == StartMenuAction::StartGameplay && gameplayController_.has_value()) {
                gameplayController_->begin(introResources_.gameplayInitialSceneName, resolveGameplaySpawnPos());
            } else if (action == StartMenuAction::Exit) {
                exitRequested = true;
            }
            continue;
        }

        if (gameplayController_.has_value()) {
            gameplayController_->runFrame(dt);
            if (easterEggManager_.shouldCloseApplication()) {
                exitRequested = true;
            }
        }
    }

    return 0;
}

bool ApplicationSession::initializeRuntime() {
    WindowInitializer::initialize(windowConfig_);
    AudioInitializer::initialize();

    AudioInitializer::loadMusicAssets(musicService_, executablePath_);
    AudioInitializer::loadSoundEffects(soundEffectService_, executablePath_);

    sceneBootstrap_ = SceneBootstrap::load(executablePath_, campusJsonPath_);
    introResources_ = IntroTourResources::load(executablePath_, sceneBootstrap_.allSceneSpawns);

    const std::string idlePath = AssetPathResolver::findPlayerIdleSprite(executablePath_);
    const std::string walkPath = AssetPathResolver::findPlayerWalkSprite(executablePath_);
    gameController_.loadPlayerSprites(idlePath, walkPath);
    gameController_.setCameraOffset(
        Vector2{windowConfig_.width * 0.5f, windowConfig_.height * 0.5f});

    return true;
}

bool ApplicationSession::loadGraphData() {
    try {
        graph_ = dataManager_.loadCampusGraph(campusJsonPath_,
                                              sceneBootstrap_.sceneToTmjPath,
                                              sceneBootstrap_.interestZonesPath,
                                              kPixelsToMeters);
    } catch (const std::exception& ex) {
        std::cerr << "Error loading GIS data: " << ex.what() << "\n";
        return false;
    }

    try {
        dataManager_.exportResolvedGraph(graph_, sceneBootstrap_.generatedGraphRuntimePath);
    } catch (const std::exception& ex) {
        std::cerr << "Warning exporting runtime graph: " << ex.what() << "\n";
    }

    return true;
}

bool ApplicationSession::initializeNavigationDomain() {
    navigationService_.emplace(graph_);
    complexityAnalyzer_.emplace(graph_);
    resilienceService_.emplace(graph_);

    destinationCatalog_.loadFromGraph(graph_, sceneBootstrap_.sceneDataMap);
    scenarioManager_.setReferenceWaypoints(
        destinationCatalog_.preferredReferenceWaypoints(graph_));

    sceneBootstrap_.buildPortalSceneLinks(transitions_, sceneLinks_);
    const auto floorLinks = FloorLinkLoader::loadFloorLinks(
        transitions_,
        sceneBootstrap_.floorScenes,
        sceneBootstrap_.allSceneSpawns,
        sceneBootstrap_.allFloorTriggers);
    sceneLinks_.insert(sceneLinks_.end(), floorLinks.begin(), floorLinks.end());

    runtimeBlockerService_.rebuildOptions(graph_, destinationCatalog_, sceneLinks_);
    transitions_.setBlockerService(&runtimeBlockerService_);
    sceneBootstrap_.buildRouteScenes(destinationCatalog_, routeScenes_);

    runtimeNavigation_.emplace(destinationCatalog_);
    tabState_ = createTabManagerState(graph_);
    routeState_ = RouteRuntimeState{};
    routeState_.routeMobilityReduced = scenarioManager_.isMobilityReduced();
    routeState_.routeAnchorPos = resolveGameplaySpawnPos();

    return true;
}

void ApplicationSession::initializeControllers() {
    gameplayController_.emplace(windowConfig_.width,
                                windowConfig_.height,
                                graph_,
                                *navigationService_,
                                scenarioManager_,
                                *complexityAnalyzer_,
                                *resilienceService_,
                                destinationCatalog_,
                                sceneLinks_,
                                inputManager_,
                                gameController_,
                                sceneManager_,
                                uiManager_,
                                *runtimeNavigation_,
                                transitions_,
                                runtimeBlockerService_,
                                musicService_,
                                soundEffectService_,
                                easterEggManager_,
                                sceneBootstrap_,
                                routeScenes_,
                                tabState_,
                                uiState_,
                                routeState_,
                                executablePath_,
                                [this](std::string sceneName) { return canonicalSceneId(std::move(sceneName)); },
                                [this](const std::string& sceneName) {
                                    return sceneBootstrap_.sceneDisplayName(sceneName, destinationCatalog_);
                                },
                                [this](const std::string& sceneName) {
                                    return sceneBootstrap_.sceneTargetPoint(sceneName);
                                });

    startMenuController_.emplace(kIntroMenuEnabled,
                                 windowConfig_.width,
                                 windowConfig_.height,
                                 introResources_,
                                 sceneBootstrap_,
                                 destinationCatalog_,
                                 sceneManager_,
                                 runtimeBlockerService_);
}

void ApplicationSession::shutdown() {
    if (!initialized_ && !IsWindowReady()) {
        return;
    }

    startMenuController_.reset();
    gameplayController_.reset();
    runtimeNavigation_.reset();
    complexityAnalyzer_.reset();
    resilienceService_.reset();
    navigationService_.reset();

    sceneManager_.unload();
    gameController_.unloadPlayerSprites();
    musicService_.unloadAll();
    soundEffectService_.unloadAll();
    AudioManager::getInstance().shutdown();

    if (IsWindowReady()) {
        WindowInitializer::cleanup();
    }

    initialized_ = false;
}

std::string ApplicationSession::canonicalSceneId(std::string sceneName) const {
    std::transform(sceneName.begin(), sceneName.end(), sceneName.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return sceneName;
}

Vector2 ApplicationSession::resolveGameplaySpawnPos() const {
    const auto sceneIt = sceneBootstrap_.allSceneSpawns.find(introResources_.gameplayInitialSceneName);
    if (sceneIt != sceneBootstrap_.allSceneSpawns.end()) {
        const auto spawnIt = sceneIt->second.find("bus_arrive");
        if (spawnIt != sceneIt->second.end()) {
            return spawnIt->second;
        }
    }

    return WalkablePathService::findSpawnPoint(sceneManager_.getMapData());
}
