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
#include "services/TransitionService.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

struct MapRenderData {
    Texture2D texture{};
    bool hasTexture{false};
    std::vector<Rectangle> hitboxes;
};

struct SceneData {
    Texture2D texture{};
    std::vector<Rectangle> hitboxes;
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

    // --- Scene definitions (paths only; transitions are handled by TransitionService) ---
    const std::vector<SceneConfig> allScenes = {
        {"Exteriorcafeteria", "assets/maps/Exteriorcafeteria.png", "assets/maps/Exteriorcafeteria.tmj"},
        {"Paradadebus",       "assets/maps/Paradadebus.png",       "assets/maps/Paradadebus.tmj"},
        {"Interiorcafeteria", "assets/maps/Interiorcafeteria.png", "assets/maps/Interiorcafeteria.tmj"},
        {"biblio",            "assets/maps/biblio.png",            "assets/maps/biblio.tmj"},
        {"piso1",             "assets/maps/piso 1.png",            "assets/maps/piso1.tmj"},
        {"piso2",             "assets/maps/piso2.png",             "assets/maps/piso2.tmj"},
        {"piso3",             "assets/maps/piso 3.png",            "assets/maps/piso3.tmj"},
        {"piso4",             "assets/maps/piso 4.png",            "assets/maps/piso 4.tmj"},
        {"piso5",             "assets/maps/piso 5.png",            "assets/maps/piso 5.tmj"},
    };

    std::unordered_map<std::string, SceneConfig> sceneMap;
    for (const auto& sc : allScenes) sceneMap[sc.name] = sc;

    // Pre-load hitboxes for all scenes into sceneDataMap
    std::unordered_map<std::string, SceneData> sceneDataMap;
    for (const auto& sc : allScenes) {
        SceneData sd;
        const std::string tmjPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, sc.tmjPath);
        if (!tmjPath.empty()) {
            try {
                sd.hitboxes = loadHitboxesFromTmj(tmjPath);
                sd.isValid = true;
            } catch (const std::exception& ex) {
                std::cerr << "No se pudo leer " << sc.tmjPath << ": " << ex.what() << "\n";
            }
        } else {
            std::cerr << "No se encontro " << sc.tmjPath << "\n";
        }
        sceneDataMap[sc.name] = std::move(sd);
    }

    // -----------------------------------------------------------------------
    // Portal & elevator definitions (bidirectional, spawnpoint-aware)
    // -----------------------------------------------------------------------
    TransitionService transitions;

    // -----------------------------------------------------------------------
    // PORTALS
    // -----------------------------------------------------------------------

    // --- Parada de bus <--> Exterior cafeteria ---
    // ExtCafe trigger: x=[1272,1272], y=[684,842]  (right-edge strip, auto)
    // Paradadebus trigger: x=[326.3,445.6], y≈14   (top edge, auto)
    // Spawn in Exteriorcafeteria: 1265, 810
    // Spawn in Paradadebus:       midpoint of top trigger (385, 35)
    transitions.addPortal({
        "ext_parada",
        "Exteriorcafeteria", "Paradadebus",
        {1272.0f, 684.0f,   1.0f, 158.0f},  // right-edge strip in Exteriorcafeteria
        { 326.3f,   9.0f, 119.3f,  10.0f},  // top edge in Paradadebus
        {1265.0f, 810.0f},                   // spawn in Exteriorcafeteria (arriving from bus stop)
        { 385.0f,  35.0f},                   // spawn in Paradadebus (arriving from exterior)
        false
    });

    // --- Exterior cafeteria <--> Piso 4 (main entrance, left half → spawn 565,515) ---
    // The main entrance x=[1113,1243] is split: left half x=[1113,1178], right half x=[1178,1243].
    // Left half: player was closer to the left → spawns at 565,515 inside piso4.
    transitions.addPortal({
        "ext_piso4_main_L",
        "Exteriorcafeteria", "piso4",
        {1113.0f, 493.0f,  65.0f,   8.0f},  // left half of main entrance in Exteriorcafeteria
        { 558.0f, 510.0f,  15.0f,   8.0f},  // return trigger in piso4 near spawn 565,515
        {1145.0f, 520.0f},                   // spawn in Exteriorcafeteria (return)
        { 565.0f, 515.0f},                   // spawn in piso4 (left entry)
        false
    });

    // --- Exterior cafeteria <--> Piso 4 (main entrance, right half → spawn 720,515) ---
    // Right half: player was closer to the right → spawns at 720,515 inside piso4.
    transitions.addPortal({
        "ext_piso4_main_R",
        "Exteriorcafeteria", "piso4",
        {1178.0f, 493.0f,  65.0f,   8.0f},  // right half of main entrance in Exteriorcafeteria
        { 713.0f, 510.0f,  15.0f,   8.0f},  // return trigger in piso4 near spawn 720,515
        {1210.0f, 520.0f},                   // spawn in Exteriorcafeteria (return)
        { 720.0f, 515.0f},                   // spawn in piso4 (right entry)
        false
    });

    // --- Exterior cafeteria <--> Piso 4 (secondary access, x≈806.9, y≈439 → spawn 27,210) ---
    transitions.addPortal({
        "ext_piso4_sec",
        "Exteriorcafeteria", "piso4",
        { 803.0f, 434.0f,   8.0f,  10.0f},  // secondary access trigger in Exteriorcafeteria
        {  20.0f, 205.0f,  15.0f,   8.0f},  // return trigger in piso4 near spawn 27,210
        { 810.0f, 460.0f},                   // spawn in Exteriorcafeteria (return)
        {  27.0f, 210.0f},                   // spawn in piso4 (secondary entry)
        false
    });

    // --- Exterior cafeteria <--> Biblioteca (requires E) ---
    // Trigger: x=[582,644.3], y=[389,395]  →  spawn in biblio: 111,535
    transitions.addPortal({
        "ext_biblio",
        "Exteriorcafeteria", "biblio",
        { 582.0f, 389.0f,  62.3f,   6.0f},  // trigger in Exteriorcafeteria
        { 104.0f, 530.0f,  14.0f,   8.0f},  // return trigger in biblio near spawn 111,535
        { 613.0f, 420.0f},                   // spawn in Exteriorcafeteria (return)
        { 111.0f, 535.0f},                   // spawn in biblio
        true                                  // requires E
    });

    // --- Exterior cafeteria <--> Interior cafeteria ---
    // Trigger: x=[260,297], y=[392,392]  →  spawn in intcafe: 800,155
    transitions.addPortal({
        "ext_intcafe",
        "Exteriorcafeteria", "Interiorcafeteria",
        { 260.0f, 388.0f,  37.0f,   8.0f},  // trigger in Exteriorcafeteria
        { 793.0f, 150.0f,  14.0f,   8.0f},  // return trigger in Interiorcafeteria near spawn 800,155
        { 278.0f, 420.0f},                   // spawn in Exteriorcafeteria (return)
        { 800.0f, 155.0f},                   // spawn in Interiorcafeteria
        false
    });

    // -----------------------------------------------------------------------
    // FLOOR ELEVATORS (3 access points per floor: elevator, left stair, right stair)
    //
    // Each access type sends the player to the matching access on the destination
    // floor:  elevator → elevator,  left stair → left stair,  right stair → right stair.
    //
    // Trigger rects use the exact pixel ranges given for each floor.
    // Spawn positions are placed at the midpoint of each access range, shifted
    // slightly below so the player lands just inside the walkable area.
    // -----------------------------------------------------------------------

    // Spawn positions indexed as: [floor index 0-4][access type 0=elevator,1=left,2=right]
    // Floors: piso1(0), piso2(1), piso3(2), piso4(3), piso5(4)
    struct FloorAccess {
        const char* scene;
        const char* label;
        Vector2 elevatorSpawn;   // spawn when arriving via elevator
        Vector2 leftStairSpawn;  // spawn when arriving via left staircase
        Vector2 rightStairSpawn; // spawn when arriving via right staircase
        Rectangle elevatorRect;  // trigger: elevator trigger for THIS floor
        Rectangle leftStairRect; // trigger: left staircase trigger for THIS floor
        Rectangle rightStairRect;// trigger: right staircase trigger for THIS floor
    };

    // Trigger rects and spawn points per floor, derived from user-provided ranges:
    //   Elevator spawn  = midpoint of "De X1,Y1 a X2,Y2" + slight y offset
    //   Stair spawn     = midpoint of stair range + slight y offset
    //   Trigger rect    = {X1, Y1-5, X2-X1, 10}  (thin strip just above the range)
    const FloorAccess floorAccess[] = {
        // piso1: elevator De 480,260 a 650,260 | left 30,190-55,190 | right 930,190-970,190
        {
            "piso1", "Piso 1",
            {565.0f, 265.0f},   // elevator spawn
            { 42.0f, 200.0f},   // left stair spawn
            {950.0f, 200.0f},   // right stair spawn
            {480.0f, 255.0f, 170.0f, 10.0f},  // elevator trigger
            { 30.0f, 185.0f,  25.0f, 10.0f},  // left stair trigger
            {930.0f, 185.0f,  40.0f, 10.0f},  // right stair trigger
        },
        // piso2: elevator De 480,240 a 655,240 | left 90,195-125,195 | right 910,190-940,190
        {
            "piso2", "Piso 2",
            {567.0f, 245.0f},   // elevator spawn
            {107.0f, 205.0f},   // left stair spawn
            {925.0f, 200.0f},   // right stair spawn
            {480.0f, 235.0f, 175.0f, 10.0f},  // elevator trigger
            { 90.0f, 190.0f,  35.0f, 10.0f},  // left stair trigger
            {910.0f, 185.0f,  30.0f, 10.0f},  // right stair trigger
        },
        // piso3: elevator De 480,240 a 655,240 | left 90,195-125,195 | right 910,190-940,190
        {
            "piso3", "Piso 3",
            {567.0f, 245.0f},   // elevator spawn
            {107.0f, 205.0f},   // left stair spawn
            {925.0f, 200.0f},   // right stair spawn
            {480.0f, 235.0f, 175.0f, 10.0f},  // elevator trigger
            { 90.0f, 190.0f,  35.0f, 10.0f},  // left stair trigger
            {910.0f, 185.0f,  30.0f, 10.0f},  // right stair trigger
        },
        // piso4: elevator De 530,220 a 700,230 | left 30,190-65,190 | right 910,190-950,190
        {
            "piso4", "Piso 4",
            {615.0f, 240.0f},   // elevator spawn
            { 47.0f, 200.0f},   // left stair spawn
            {930.0f, 200.0f},   // right stair spawn
            {530.0f, 215.0f, 170.0f, 20.0f},  // elevator trigger
            { 30.0f, 185.0f,  35.0f, 10.0f},  // left stair trigger
            {910.0f, 185.0f,  40.0f, 10.0f},  // right stair trigger
        },
        // piso5: elevator De 480,240 a 655,240 | left 95,190-130,190 | right 910,190-940,190
        {
            "piso5", "Piso 5",
            {567.0f, 245.0f},   // elevator spawn
            {112.0f, 200.0f},   // left stair spawn
            {925.0f, 200.0f},   // right stair spawn
            {480.0f, 235.0f, 175.0f, 10.0f},  // elevator trigger
            { 95.0f, 185.0f,  35.0f, 10.0f},  // left stair trigger
            {910.0f, 185.0f,  30.0f, 10.0f},  // right stair trigger
        },
    };
    constexpr int kNumFloors = 5;

    // For each floor, register 3 FloorElevator instances (one per access type).
    // Each instance lists all OTHER floors with the corresponding spawn position.
    for (int i = 0; i < kNumFloors; ++i) {
        // --- Elevator ---
        {
            FloorElevator fe;
            fe.id          = std::string("elevator_") + floorAccess[i].scene;
            fe.scene       = floorAccess[i].scene;
            fe.triggerRect = floorAccess[i].elevatorRect;
            for (int j = 0; j < kNumFloors; ++j) {
                if (j == i) continue;
                fe.floors.push_back({floorAccess[j].scene, floorAccess[j].elevatorSpawn, floorAccess[j].label});
            }
            transitions.addFloorElevator(fe);
        }
        // --- Left staircase ---
        {
            FloorElevator fe;
            fe.id          = std::string("stair_left_") + floorAccess[i].scene;
            fe.scene       = floorAccess[i].scene;
            fe.triggerRect = floorAccess[i].leftStairRect;
            for (int j = 0; j < kNumFloors; ++j) {
                if (j == i) continue;
                fe.floors.push_back({floorAccess[j].scene, floorAccess[j].leftStairSpawn, floorAccess[j].label});
            }
            transitions.addFloorElevator(fe);
        }
        // --- Right staircase ---
        {
            FloorElevator fe;
            fe.id          = std::string("stair_right_") + floorAccess[i].scene;
            fe.scene       = floorAccess[i].scene;
            fe.triggerRect = floorAccess[i].rightStairRect;
            for (int j = 0; j < kNumFloors; ++j) {
                if (j == i) continue;
                fe.floors.push_back({floorAccess[j].scene, floorAccess[j].rightStairSpawn, floorAccess[j].label});
            }
            transitions.addFloorElevator(fe);
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
        if (!pngPath.empty()) {
            mapData.texture = LoadTexture(pngPath.c_str());
            mapData.hasTexture = mapData.texture.id != 0;
        } else {
            std::cerr << "No se encontro " << initConfig.pngPath << "\n";
        }
        const auto sdIt = sceneDataMap.find(initialSceneName);
        if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
            mapData.hitboxes = sdIt->second.hitboxes;
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

    // Scene state
    std::string currentSceneName = initialSceneName;

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

        // --- Scene transition (portal & elevator detection + fade state machine) ---
        transitions.update(playerColliderAt(playerPos), currentSceneName, dt);

        // Perform scene swap at peak blackness (alpha == 1.0)
        if (transitions.needsSceneSwap()) {
            const TransitionRequest req = transitions.getPendingSwap();
            if (mapData.hasTexture) {
                UnloadTexture(mapData.texture);
                mapData.texture    = {};
                mapData.hasTexture = false;
            }
            mapData.hitboxes.clear();

            const auto scIt = sceneMap.find(req.targetScene);
            if (scIt != sceneMap.end()) {
                const std::string pngPath = resolveAssetPath(argc > 0 ? argv[0] : nullptr, scIt->second.pngPath);
                if (!pngPath.empty()) {
                    mapData.texture    = LoadTexture(pngPath.c_str());
                    mapData.hasTexture = mapData.texture.id != 0;
                }
                const auto sdIt = sceneDataMap.find(req.targetScene);
                if (sdIt != sceneDataMap.end() && sdIt->second.isValid) {
                    mapData.hitboxes = sdIt->second.hitboxes;
                }
                currentSceneName = req.targetScene;
            }
            playerPos      = req.spawnPos;
            camera.target  = playerPos;
            camera.zoom    = 2.2f;
            clampCameraTarget(camera, mapData, screenWidth, screenHeight);
            transitions.notifySwapDone();
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

        // Debug: draw portal trigger zones as green semi-transparent rectangles
        if (showTriggers) {
            for (const auto& portal : transitions.getPortals()) {
                auto drawZone = [](const Rectangle& r) {
                    DrawRectangleRec(r, Color{0, 255, 0, 60});
                    DrawRectangleLinesEx(r, 1.5f, Color{0, 255, 0, 180});
                };
                if (portal.sceneA == currentSceneName) drawZone(portal.triggerA);
                if (portal.sceneB == currentSceneName) drawZone(portal.triggerB);
            }
            for (const auto& elev : transitions.getElevators()) {
                if (elev.scene == currentSceneName) {
                    DrawRectangleRec(elev.triggerRect, Color{0, 180, 255, 60});
                    DrawRectangleLinesEx(elev.triggerRect, 1.5f, Color{0, 180, 255, 180});
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

        // --- Fade overlay (screen space) ---
        transitions.drawFadeOverlay(screenWidth, screenHeight);

        // --- "Presiona E" prompt ---
        if (transitions.isPromptVisible()) {
            const char* hint        = transitions.getPromptHint().c_str();
            const int   hintFontSz  = 22;
            const int   hintW       = MeasureText(hint, hintFontSz);
            const int   hintX       = (screenWidth  - hintW) / 2;
            const int   hintY       = screenHeight - 60;
            DrawRectangle(hintX - 10, hintY - 8, hintW + 20, hintFontSz + 16,
                          Color{0, 0, 0, 180});
            DrawText(hint, hintX, hintY, hintFontSz, YELLOW);
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

        // Floor-elevator menu (shown when player presses E near an elevator)
        transitions.drawFloorMenu();

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
