#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1506-1534, 1609-1620 (movimiento y animación del jugador)
// Responsabilidad: Gestionar input de movimiento, colisiones y animación del jugador

#include <raylib.h>
#include "core/runtime/SceneRuntimeTypes.h"
#include "runtime/RuntimeInputManager.h"

struct PlayerState {
    Vector2 position{0.0f, 0.0f};
    float speed{150.0f};
    float sprintMultiplier{1.8f};
    float renderScale{1.6f};
    int direction{0};
    bool isMoving{false};
    bool sprinting{false};
};

class RuntimePlayerManager {
public:
    // MIGRADO DESDE main.cpp:1506-1534 (player movement with collision)
    static void updateMovement(PlayerState& player,
                               const RuntimeInputState& inputState,
                               const MapRenderData& mapData,
                               float dt);
    
    // MIGRADO DESDE main.cpp:1609-1620 (animation frame update)
    static void updateAnimation(SpriteAnim& playerAnim, float dt);
    
    // MIGRADO DESDE main.cpp:1696-1721 (render player sprite/circle)
    static void render(const SpriteAnim& playerAnim, const Vector2& playerPos, float renderScale);
    
    // Helper
    static Vector2 getFrontAnchor(const Vector2& playerPos, int direction);
};
