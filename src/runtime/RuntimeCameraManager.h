#pragma once
// MIGRADO DESDE main.cpp: Líneas ~636-655 (clampCameraTarget)
// Responsabilidad: Gestionar cámara 2D, zoom y clamping

#include <raylib.h>
#include "core/runtime/SceneRuntimeTypes.h"

class RuntimeCameraManager {
public:
    // MIGRADO DESDE main.cpp:636-655
    static void clampCameraTarget(Camera2D& camera, const MapRenderData& mapData, int screenWidth, int screenHeight);
    
    // Configuración inicial de cámara
    static Camera2D createCamera(int screenWidth, int screenHeight, const Vector2& target);
    
    // Aplicar zoom con rueda del ratón
    static void applyZoom(Camera2D& camera, float wheelDelta, float minZoom, float maxZoom);
    
    // Actualizar target de cámara siguiendo al jugador
    static void followTarget(Camera2D& camera, const Vector2& playerPos);
};
