#pragma once
// MIGRADO DESDE main.cpp: Líneas ~1823-1846 (fade overlay, prompt E, coordenadas)
// Responsabilidad: Gestionar UI fija en pantalla (coordenadas, hints, overlays de transición)

#include <raylib.h>
#include <string>
#include "core/runtime/SceneRuntimeTypes.h"
#include "services/TransitionService.h"

class RuntimeUiManager {
public:
    // MIGRADO DESDE main.cpp:1823-1824 (fade overlay)
    static void drawFadeOverlay(const TransitionService& transitions, int screenWidth, int screenHeight);
    
    // MIGRADO DESDE main.cpp:1826-1837 (prompt "Presiona E")
    static void drawEPrompt(const TransitionService& transitions, int screenWidth, int screenHeight);
    
    // MIGRADO DESDE main.cpp:1839-1848 (coordenadas y hints fijos)
    static void drawFixedHUD(const Vector2& playerPos, int screenWidth, int screenHeight, bool routeActive);
};
