#include <raylib.h>
#include "rlImGui.h"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <cstdio>
#include <cmath>
#include <nlohmann/json.hpp>
#include "repositories/JsonGraphRepository.h"
#include "services/NavigationService.h"
#include "services/ScenarioManager.h"
#include "services/ComplexityAnalyzer.h"
#include "services/ResilienceService.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

struct MapRenderData {
    Texture2D texture{};
    bool hasTexture{false};
    std::vector<Rectangle> hitboxes;
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

struct SceneTrigger {
    std::string name;
    Rectangle zone{};
    std::string targetScene;
    Vector2 spawnPos{};
    bool requiresKeyE{false};
    char activationKey{'E'};
};

struct SceneConfig {
    std::string name;
    std::string pngPath;
    std::string tmjPath;
    std::vector<SceneTrigger> triggers;
};

static int directionStartFrame(int direction, int totalFrames) {
    if (totalFrames < 4) return 0;
    const int framesPerDir = std::max(1, totalFrames / 4);
    const int safeDir = std::clamp(direction, 0, 3);
    return safeDir * framesPerDir;
}

static int directionalFrameCount(int totalFrames) {
    if (totalFrames < 4) return std::max(1, totalFrames);
    return std::max(1, totalFrames / 4);
}

static std::string findPathCandidate(const char* argv0, const std::vector<fs::path>& baseCandidates) {
    std::vector<fs::path> candidates = baseCandidates;

    if (argv0 && argv0[0] != '\0') {
        const fs::path exePath = fs::absolute(argv0).parent_path();
        for (const auto& base : baseCandidates) {
            candidates.emplace_back(exePath / base);
            candidates.emplace_back(exePath / ".." / base);
            candidates.emplace_back(exePath / "../.." / base);
        }
    }

    for (const auto& c : candidates) {
        if (fs::exists(c)) return c.string();
    }
    return "";
}

static std::string findCampusJson(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("campus.json"),
        fs::path("../campus.json"),
        fs::path("../../campus.json"),
        fs::path("../EcoCampusNav/campus.json")
    });
}

static std::string resolveAssetPath(const char* argv0, const std::string& relPath) {
    return findPathCandidate(argv0, {
        fs::path(relPath),
        fs::path("..") / relPath,
        fs::path("../..") / relPath
    });
}

static std::string findPlayerIdleSprite(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("assets/sprites/m_Character/junior_AnguloIdle.png"),
        fs::path("../assets/sprites/m_Character/junior_AnguloIdle.png"),
        fs::path("../../assets/sprites/m_Character/junior_AnguloIdle.png")
    });
}

static std::string findPlayerWalkSprite(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("assets/sprites/m_Character/junior_AnguloWalk.png"),
        fs::path("../assets/sprites/m_Character/junior_AnguloWalk.png"),
        fs::path("../../assets/sprites/m_Character/junior_AnguloWalk.png")
    });
}

static std::vector<Rectangle> loadHitboxesFromTmj(const std::string& tmjPath) {
    std::ifstream file(tmjPath);
    if (!file.is_open()) return {};

    json tmj;
    file >> tmj;

    std::vector<Rectangle> hitboxes;
    if (!tmj.contains("layers") || !tmj["layers"].is_array()) return hitboxes;

    for (const auto& layer : tmj["layers"]) {
        if (!layer.contains("type") || layer["type"] != "objectgroup") continue;
        if (layer.contains("name") && layer["name"] != "Hitboxes") continue;
        if (!layer.contains("objects") || !layer["objects"].is_array()) continue;

        for (const auto& obj : layer["objects"]) {
            if (!obj.contains("x") || !obj.contains("y") || !obj.contains("width") || !obj.contains("height")) continue;

            Rectangle r{};
            r.x = obj["x"].get<float>();
            r.y = obj["y"].get<float>();
            r.width = obj["width"].get<float>();
            r.height = obj["height"].get<float>();
            hitboxes.push_back(r);
        }
    }

    return hitboxes;
}

static bool intersectsAny(const Rectangle& rect, const std::vector<Rectangle>& obstacles) {
    for (const auto& obstacle : obstacles) {
        if (CheckCollisionRecs(rect, obstacle)) return true;
    }
    return false;
}

static Rectangle playerColliderAt(const Vector2& playerPos) {
    Rectangle collider{};
    // Huella de colision en los pies para movimiento top-down.
    collider.x = playerPos.x - 5.0f;
    collider.y = playerPos.y - 8.0f;
    collider.width = 10.0f;
    collider.height = 8.0f;
    return collider;
}

static Vector2 findSpawnPoint(const MapRenderData& mapData) {
    if (!mapData.hasTexture) return {100.0f, 100.0f};

    const float maxX = static_cast<float>(mapData.texture.width) - 20.0f;
    const float maxY = static_cast<float>(mapData.texture.height) - 20.0f;

    // Priorizar spawn en esquina inferior derecha (zona parada de bus).
    const float preferredX = std::clamp(maxX * 0.88f, 20.0f, maxX);
    const float preferredY = std::clamp(maxY * 0.90f, 40.0f, maxY);
    for (float radius = 0.0f; radius <= 240.0f; radius += 16.0f) {
        for (float dy = -radius; dy <= radius; dy += 16.0f) {
            for (float dx = -radius; dx <= radius; dx += 16.0f) {
                const float x = std::clamp(preferredX + dx, 20.0f, maxX);
                const float y = std::clamp(preferredY + dy, 40.0f, maxY);
                const Vector2 p{x, y};
                if (!intersectsAny(playerColliderAt(p), mapData.hitboxes)) return p;
            }
        }
    }

    // Fallback general.
    for (float y = maxY * 0.5f; y < maxY; y += 16.0f) {
        for (float x = 20.0f; x < maxX; x += 16.0f) {
            const Vector2 p{x, y};
            if (!intersectsAny(playerColliderAt(p), mapData.hitboxes)) return p;
        }
    }
    return {static_cast<float>(mapData.texture.width) * 0.5f, static_cast<float>(mapData.texture.height) * 0.5f};
}

static void drawMapWithHitboxes(const MapRenderData& mapData, bool showHitboxes) {
    if (!mapData.hasTexture) return;
    DrawTexture(mapData.texture, 0, 0, WHITE);

    if (!showHitboxes) return;

    for (const auto& h : mapData.hitboxes) {
        DrawRectangleLinesEx(h, 1.5f, Color{255, 80, 80, 220});
    }
}

static void clampCameraTarget(Camera2D& camera, const MapRenderData& mapData, int screenWidth, int screenHeight) {
    if (!mapData.hasTexture) return;
    const float halfViewWidth = (static_cast<float>(screenWidth) * 0.5f) / camera.zoom;
    const float halfViewHeight = (static_cast<float>(screenHeight) * 0.5f) / camera.zoom;
    const float minX = halfViewWidth;
    const float maxX = static_cast<float>(mapData.texture.width) - halfViewWidth;
    const float minY = halfViewHeight;
    const float maxY = static_cast<float>(mapData.texture.height) - halfViewHeight;

    if (minX > maxX) {
        camera.target.x = static_cast<float>(mapData.texture.width) * 0.5f;
    } else {
        camera.target.x = std::clamp(camera.target.x, minX, maxX);
    }
    if (minY > maxY) {
        camera.target.y = static_cast<float>(mapData.texture.height) * 0.5f;
    } else {
        camera.target.y = std::clamp(camera.target.y, minY, maxY);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;

    std::string path = findCampusJson(argc > 0 ? argv[0] : nullptr);
    if (path.empty()) {
        std::cerr << "No se encontro campus.json. Coloque el archivo en el directorio de trabajo.\n";
        return 1;
    }

    CampusGraph graph;
    try {
        graph = JsonGraphRepository::loadFromFile(path);
    } catch (const std::exception& ex) {
        std::cerr << "Error al cargar datos: " << ex.what() << "\n";
        return 1;
    }

    NavigationService nav_service(graph);
    ScenarioManager scenario_manager;
    ComplexityAnalyzer complexity_analyzer(graph);
    ResilienceService resilience_service(graph);

    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "EcoCampusNav (Raylib)");
    SetTargetFPS(60);

    rlImGuiSetup(true);

    // --- Scene definitions ---
    const std::vector<SceneConfig> allScenes = {
        {
            "Exteriorcafeteria",
            "assets/maps/Exteriorcafeteria.png",
            "assets/maps/Exteriorcafeteria.tmj",
            {
                {"VueltaParada",        {1272.0f, 684.0f,  40.0f, 160.0f}, "Paradadebus",       {350.0f, 600.0f}, false},
                {"EntradaPiso4",        {1113.0f, 497.0f, 130.0f,  10.0f}, "piso4",             {400.0f, 400.0f}, false},
                {"EntradaBiblio",       { 582.0f, 389.0f,  62.0f,  10.0f}, "biblio",            {200.0f, 200.0f}, true},
                {"AccesoPiso4_2",       { 800.0f, 430.0f,  20.0f,  20.0f}, "piso4",             {400.0f, 400.0f}, false},
                {"EntradaInteriorCafe", { 260.0f, 392.0f,  40.0f,  10.0f}, "Interiorcafeteria", {400.0f, 500.0f}, false},
            }
        },
        {
            "Paradadebus",
            "assets/maps/Paradadebus.png",
            "assets/maps/Paradadebus.tmj",
            {
                // Bus stop exit zone at the top edge of the map
                {"SalidaParada", {326.3f, 14.0f, 119.3f, 20.0f}, "Exteriorcafeteria", {600.0f, 600.0f}, false},
            }
        },
        {"Interiorcafeteria", "assets/maps/Interiorcafeteria.png", "assets/maps/Interiorcafeteria.tmj", {}},
        {"biblio",            "assets/maps/biblio.png",            "assets/maps/biblio.tmj",            {}},
        {"piso1",             "assets/maps/piso 1.png",            "assets/maps/piso1colisiones.tmj",   {}},
        {"piso2",             "assets/maps/piso2.png",             "assets/maps/piso2colisiones.tmj",   {}},
        {"piso3",             "assets/maps/piso 3.png",            "assets/maps/piso3colisiones.tmj",   {}},
        {"piso4",             "assets/maps/piso 4.png",            "assets/maps/piso 4.tmj",            {}},
        {"piso5",             "assets/maps/piso 5.png",            "assets/maps/piso 5colisiones.tmj",  {}},
    };

    std::unordered_map<std::string, SceneConfig> sceneMap;
    for (const auto& sc : allScenes) sceneMap[sc.name] = sc;

    // Validate trigger target references at startup
    for (const auto& sc : allScenes) {
        for (const auto& trigger : sc.triggers) {
            if (sceneMap.find(trigger.targetScene) == sceneMap.end()) {
                std::cerr << "Advertencia: trigger '" << trigger.name
                          << "' en escena '" << sc.name
                          << "' apunta a escena desconocida '" << trigger.targetScene << "'\n";
            }
        }
    }

    // Load initial scene
    const std::string initialSceneName = "Paradadebus";
    MapRenderData mapData;
    bool showHitboxes = false;
    bool showTriggers = false;
    {
        const auto& initConfig = sceneMap.at(initialSceneName);
        const std::string pngPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.pngPath);
        const std::string tmjPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, initConfig.tmjPath);
        if (!pngPath.empty()) {
            mapData.texture = LoadTexture(pngPath.c_str());
            mapData.hasTexture = mapData.texture.id != 0;
        } else {
            std::cerr << "No se encontro " << initConfig.pngPath << "\n";
        }
        if (!tmjPath.empty()) {
            try {
                mapData.hitboxes = loadHitboxesFromTmj(tmjPath);
            } catch (const std::exception& ex) {
                std::cerr << "No se pudo leer " << initConfig.tmjPath << ": " << ex.what() << "\n";
            }
        } else {
            std::cerr << "No se encontro " << initConfig.tmjPath << "\n";
        }
    }

    SpriteAnim playerAnim;
    const std::string idlePath = findPlayerIdleSprite(argc > 0 ? argv[0] : nullptr);
    const std::string walkPath = findPlayerWalkSprite(argc > 0 ? argv[0] : nullptr);
    if (!idlePath.empty()) {
        playerAnim.idle = LoadTexture(idlePath.c_str());
        playerAnim.hasIdle = playerAnim.idle.id != 0;
    }
    if (!walkPath.empty()) {
        playerAnim.walk = LoadTexture(walkPath.c_str());
        playerAnim.hasWalk = playerAnim.walk.id != 0;
    }

    if (playerAnim.hasIdle) {
        playerAnim.frameHeight = playerAnim.idle.height;
        playerAnim.frameWidth = 16;
        if (playerAnim.idle.width % playerAnim.frameWidth != 0) {
            playerAnim.frameWidth = std::max(1, playerAnim.idle.height);
        }
        playerAnim.idleFrames = std::max(1, playerAnim.idle.width / playerAnim.frameWidth);
    }
    if (playerAnim.hasWalk) {
        if (playerAnim.walk.width % playerAnim.frameWidth != 0) {
            playerAnim.frameWidth = std::max(1, playerAnim.walk.height);
        }
        playerAnim.walkFrames = std::max(1, playerAnim.walk.width / std::max(1, playerAnim.frameWidth));
    }

    Vector2 playerPos = findSpawnPoint(mapData);
    const float playerSpeed = 150.0f;
    const float sprintMultiplier = 1.8f;
    const float playerRenderScale = 1.6f;
    playerAnim.direction = 0;
    Camera2D camera{};
    camera.offset = Vector2{screenWidth * 0.5f, screenHeight * 0.5f};
    camera.target = playerPos;
    camera.rotation = 0.0f;
    camera.zoom = 2.2f;
    const float minZoom = 1.2f;
    const float maxZoom = 4.0f;

    // Scene transition state
    std::string currentSceneName = initialSceneName;
    bool isTransitioning = false;
    float fadeAlpha = 0.0f;
    bool isFadingOut = false;  // true = black overlay is fading out (scene returning to visible)
    std::string pendingTargetScene;
    Vector2 pendingSpawnPos{};
    const float fadeRate = 1.0f / 1.25f;  // 1.25s per half (2.5s total)

    std::vector<std::string> nodeIds = graph.nodeIds();
    std::string defaultStart = nodeIds.empty() ? "" : nodeIds.front();
    std::string defaultEnd = nodeIds.size() > 1 ? nodeIds[1] : defaultStart;

    char startId[64] = {0};
    char endId[64] = {0};
    std::snprintf(startId, sizeof(startId), "%s", defaultStart.c_str());
    std::snprintf(endId, sizeof(endId), "%s", defaultEnd.c_str());

    TraversalResult lastTraversal;
    PathResult lastPath;
    bool hasTraversal = false;
    bool hasPath = false;
    bool lastConnected = false;
    std::vector<AlgorithmStats> lastStats;
    std::string lastAction;

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            camera.zoom = std::clamp(camera.zoom + wheel * 0.15f, minZoom, maxZoom);
        }

        float moveX = 0.0f;
        float moveY = 0.0f;
        if (IsKeyDown(KEY_W)) moveY -= 1.0f;
        if (IsKeyDown(KEY_S)) moveY += 1.0f;
        if (IsKeyDown(KEY_A)) moveX -= 1.0f;
        if (IsKeyDown(KEY_D)) moveX += 1.0f;

        if (moveX != 0.0f || moveY != 0.0f) {
            const float len = std::sqrt(moveX * moveX + moveY * moveY);
            moveX /= len;
            moveY /= len;
        }

        // Orden real de bloques en este spritesheet (4 direcciones):
        // 0=right, 1=up, 2=left, 3=down
        if (IsKeyDown(KEY_W)) {
            playerAnim.direction = 1; // up
        } else if (IsKeyDown(KEY_S)) {
            playerAnim.direction = 3; // down
        } else if (IsKeyDown(KEY_A)) {
            playerAnim.direction = 2; // left
        } else if (IsKeyDown(KEY_D)) {
            playerAnim.direction = 0; // right
        }
        const bool sprinting = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const float currentSpeed = playerSpeed * (sprinting ? sprintMultiplier : 1.0f);

        Vector2 candidate = playerPos;
        candidate.x += moveX * currentSpeed * dt;
        if (mapData.hasTexture) {
            candidate.x = std::clamp(candidate.x, 8.0f, static_cast<float>(mapData.texture.width) - 8.0f);
        }
        if (!intersectsAny(playerColliderAt(candidate), mapData.hitboxes)) {
            playerPos.x = candidate.x;
        }

        candidate = playerPos;
        candidate.y += moveY * currentSpeed * dt;
        if (mapData.hasTexture) {
            candidate.y = std::clamp(candidate.y, 14.0f, static_cast<float>(mapData.texture.height));
        }
        if (!intersectsAny(playerColliderAt(candidate), mapData.hitboxes)) {
            playerPos.y = candidate.y;
        }
        camera.target = playerPos;
        clampCameraTarget(camera, mapData, screenWidth, screenHeight);

        // --- Scene transition trigger detection ---
        if (!isTransitioning) {
            const auto it = sceneMap.find(currentSceneName);
            if (it != sceneMap.end()) {
                const Rectangle playerCollider = playerColliderAt(playerPos);
                for (const auto& trigger : it->second.triggers) {
                    if (CheckCollisionRecs(playerCollider, trigger.zone)) {
                        const bool activated = !trigger.requiresKeyE || IsKeyPressed((KeyboardKey)trigger.activationKey);
                        if (activated) {
                            pendingTargetScene = trigger.targetScene;
                            pendingSpawnPos = trigger.spawnPos;
                            isTransitioning = true;
                            isFadingOut = false;
                            fadeAlpha = 0.0f;
                            break;
                        }
                    }
                }
            }
        }

        // --- Fade update ---
        if (isTransitioning) {
            if (!isFadingOut) {
                fadeAlpha += fadeRate * dt;
                if (fadeAlpha >= 1.0f) {
                    fadeAlpha = 1.0f;
                    // Swap scene at peak blackness
                    if (mapData.hasTexture) {
                        UnloadTexture(mapData.texture);
                        mapData.texture = {};
                        mapData.hasTexture = false;
                    }
                    mapData.hitboxes.clear();

                    const auto scIt = sceneMap.find(pendingTargetScene);
                    if (scIt != sceneMap.end()) {
                        const std::string pngPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, scIt->second.pngPath);
                        const std::string tmjPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, scIt->second.tmjPath);
                        if (!pngPath.empty()) {
                            mapData.texture = LoadTexture(pngPath.c_str());
                            mapData.hasTexture = mapData.texture.id != 0;
                        }
                        if (!tmjPath.empty()) {
                            try {
                                mapData.hitboxes = loadHitboxesFromTmj(tmjPath);
                            } catch (const std::exception& ex) {
                                std::cerr << "No se pudo leer " << scIt->second.tmjPath << ": " << ex.what() << "\n";
                            }
                        }
                        currentSceneName = pendingTargetScene;
                    }
                    playerPos = pendingSpawnPos;
                    camera.target = playerPos;
                    camera.zoom = 2.2f;
                    clampCameraTarget(camera, mapData, screenWidth, screenHeight);
                    isFadingOut = true;
                }
            } else {
                fadeAlpha -= fadeRate * dt;
                if (fadeAlpha <= 0.0f) {
                    fadeAlpha = 0.0f;
                    isTransitioning = false;
                    isFadingOut = false;
                }
            }
        }

        const bool isMoving = (moveX != 0.0f || moveY != 0.0f);
        if (isMoving) {
            playerAnim.timer += dt;
            const float frameStep = sprinting ? (1.0f / 16.0f) : (1.0f / 12.0f);
            if (playerAnim.timer >= frameStep) {
                playerAnim.timer = 0.0f;
                playerAnim.frame = (playerAnim.frame + 1) % directionalFrameCount(playerAnim.walkFrames);
            }
        } else {
            playerAnim.timer = 0.0f;
            playerAnim.frame = 0;
        }

        BeginDrawing();
        ClearBackground({18, 20, 28, 255});
        BeginMode2D(camera);
        if (mapData.hasTexture) {
            drawMapWithHitboxes(mapData, showHitboxes);
        } else {
            DrawRectangle(0, 0, screenWidth, screenHeight, {22, 26, 36, 255});
        }

        // Debug: draw trigger zones as green semi-transparent rectangles
        if (showTriggers) {
            const auto scIt = sceneMap.find(currentSceneName);
            if (scIt != sceneMap.end()) {
                for (const auto& trigger : scIt->second.triggers) {
                    DrawRectangleRec(trigger.zone, Color{0, 255, 0, 60});
                    DrawRectangleLinesEx(trigger.zone, 1.5f, Color{0, 255, 0, 180});
                }
            }
        }

        const bool useWalk = isMoving && playerAnim.hasWalk;
        const bool canDrawSprite = playerAnim.hasIdle || playerAnim.hasWalk;
        if (canDrawSprite) {
            const Texture2D tex = useWalk ? playerAnim.walk : playerAnim.idle;
            const int activeFrames = useWalk ? playerAnim.walkFrames : playerAnim.idleFrames;
            const int baseFrame = directionStartFrame(playerAnim.direction, activeFrames);
            const int frameCount = directionalFrameCount(activeFrames);
            const int activeFrame = baseFrame + (playerAnim.frame % frameCount);
            const float frameW = static_cast<float>(playerAnim.frameWidth);
            const float frameH = static_cast<float>(playerAnim.frameHeight);
            Rectangle src{
                frameW * static_cast<float>(activeFrame),
                0.0f,
                frameW,
                frameH
            };
            Rectangle dst{
                playerPos.x,
                playerPos.y,
                frameW * playerRenderScale,
                frameH * playerRenderScale
            };
            DrawTexturePro(tex, src, dst, Vector2{dst.width * 0.5f, dst.height}, 0.0f, WHITE);
        } else {
            DrawCircleV(playerPos, 8.0f, RED);
        }
        EndMode2D();

        // --- Fade overlay ---
        if (fadeAlpha > 0.0f) {
            DrawRectangle(0, 0, screenWidth, screenHeight,
                          Color{0, 0, 0, static_cast<unsigned char>(fadeAlpha * 255.0f)});
        }

        const char* coordText = TextFormat("Pos: (%.1f, %.1f)", playerPos.x, playerPos.y);
        const int coordFontSize = 20;
        const int coordPadding = 12;
        const int coordWidth = MeasureText(coordText, coordFontSize);
        const int coordX = screenWidth - coordWidth - coordPadding;
        const int coordY = coordPadding;
        DrawRectangle(coordX - 8, coordY - 6, coordWidth + 16, coordFontSize + 12, Color{0, 0, 0, 140});
        DrawText(coordText, coordX, coordY, coordFontSize, RAYWHITE);

        rlImGuiBegin();

        ImGui::SetNextWindowSize(ImVec2(360, 360), ImGuiCond_FirstUseEver);
        ImGui::Begin("Control Panel");

        ImGui::Text("Escena: %s", currentSceneName.c_str());
        ImGui::Checkbox("Hitboxes", &showHitboxes);
        ImGui::SameLine();
        ImGui::Checkbox("Triggers", &showTriggers);
        ImGui::Separator();

        bool mobilityReduced = scenario_manager.isMobilityReduced();
        if (ImGui::Checkbox("Movilidad reducida", &mobilityReduced)) {
            scenario_manager.setMobilityReduced(mobilityReduced);
        }

        int studentType = scenario_manager.getStudentType() == StudentType::NEW_STUDENT ? 0 : 1;
        if (ImGui::RadioButton("Nuevo", studentType == 0)) {
            scenario_manager.setStudentType(StudentType::NEW_STUDENT);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Regular", studentType == 1)) {
            scenario_manager.setStudentType(StudentType::REGULAR_STUDENT);
        }

        ImGui::InputText("Inicio", startId, sizeof(startId));
        ImGui::InputText("Destino", endId, sizeof(endId));

        if (ImGui::Button("DFS")) {
            lastTraversal = nav_service.runDfs(startId, scenario_manager.isMobilityReduced());
            hasTraversal = true;
            lastAction = "DFS";
        }
        ImGui::SameLine();
        if (ImGui::Button("BFS")) {
            lastTraversal = nav_service.runBfs(startId, scenario_manager.isMobilityReduced());
            hasTraversal = true;
            lastAction = "BFS";
        }

        if (ImGui::Button("Camino")) {
            lastPath = nav_service.findPath(startId, endId, scenario_manager.isMobilityReduced());
            hasPath = true;
            lastAction = "Path";
        }
        ImGui::SameLine();
        if (ImGui::Button("Conectividad")) {
            lastConnected = nav_service.checkConnectivity();
            lastAction = "Connectivity";
        }

        if (ImGui::Button("Alterno")) {
            lastPath = resilience_service.findAlternatePath(startId, endId, scenario_manager.isMobilityReduced());
            hasPath = true;
            lastAction = "AltPath";
        }
        ImGui::SameLine();
        if (ImGui::Button("Bloquear")) {
            resilience_service.blockEdge(startId, endId);
        }
        ImGui::SameLine();
        if (ImGui::Button("Desbloquear")) {
            resilience_service.unblockAll();
        }

        if (ImGui::Button("Complejidad")) {
            lastStats = complexity_analyzer.analyze(startId, scenario_manager.isMobilityReduced());
            lastAction = "Complexity";
        }

        ImGui::Separator();
        ImGui::Text("Ultima accion: %s", lastAction.c_str());

        if (hasTraversal) {
            ImGui::Text("Visitados: %d", lastTraversal.nodes_visited);
            ImGui::Text("Tiempo: %lld us", lastTraversal.elapsed_us);
        }
        if (hasPath) {
            ImGui::Text("Camino encontrado: %s", lastPath.found ? "si" : "no");
            ImGui::Text("Peso total: %.2f", lastPath.total_weight);
        }
        if (lastAction == "Connectivity") {
            ImGui::Text("Conectado: %s", lastConnected ? "si" : "no");
        }
        if (lastAction == "Complexity") {
            for (const auto& stat : lastStats) {
                ImGui::Text("%s: %d nodos, %lld us", stat.algorithm.c_str(), stat.nodes_visited, stat.elapsed_us);
            }
        }

        ImGui::End();
        rlImGuiEnd();

        EndDrawing();

    }

    if (mapData.hasTexture) {
        UnloadTexture(mapData.texture);
    }
    if (playerAnim.hasIdle) UnloadTexture(playerAnim.idle);
    if (playerAnim.hasWalk) UnloadTexture(playerAnim.walk);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
