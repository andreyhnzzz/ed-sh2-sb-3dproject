#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1399-1424, 1609-1620 (SpriteAnim y animación del jugador)
// Responsabilidad: Gestionar animaciones del jugador (idle, walk, direcciones)

#include <raylib.h>
#include <string>

struct SpriteAnim {
    Texture2D idle{};
    Texture2D walk{};
    bool hasIdle{false};
    bool hasWalk{false};
    int frameWidth{0};
    int frameHeight{0};
    int idleFrames{0};
    int walkFrames{0};
    int frame{0};
    int direction{0}; // 0=down, 1=left, 2=right, 3=up
    float timer{0.0f};
};

class RuntimeAnimationManager {
public:
    // MIGRADO DESDE main.cpp:1400-1424
    static void loadPlayerSprites(SpriteAnim& anim, const std::string& idlePath, const std::string& walkPath);
    
    // MIGRADO DESDE main.cpp:1609-1620
    static void update(SpriteAnim& anim, bool isMoving, float dt, bool sprinting);
    
    // MIGRADO DESDE main.cpp:1696-1721
    static void render(const SpriteAnim& anim, const Vector2& playerPos, float renderScale);
    
    // Liberar recursos
    static void unload(SpriteAnim& anim);
};
