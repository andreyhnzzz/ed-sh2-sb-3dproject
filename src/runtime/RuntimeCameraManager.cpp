#include "RuntimeCameraManager.h"
#include <algorithm>

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~636-655 (clampCameraTarget)
// ============================================================================

void RuntimeCameraManager::clampCameraTarget(Camera2D& camera, const MapRenderData& mapData, int screenWidth, int screenHeight) {
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

// ============================================================================
// NUEVO: Creación de cámara inicial (extraído de main.cpp:1439-1443)
// ============================================================================

Camera2D RuntimeCameraManager::createCamera(int screenWidth, int screenHeight, const Vector2& target) {
    Camera2D camera{};
    camera.offset = Vector2{screenWidth * 0.5f, screenHeight * 0.5f};
    camera.target = target;
    camera.rotation = 0.0f;
    camera.zoom = 2.2f;
    return camera;
}

// ============================================================================
// NUEVO: Aplicar zoom (extraído de main.cpp:1501-1504)
// ============================================================================

void RuntimeCameraManager::applyZoom(Camera2D& camera, float wheelDelta, float minZoom, float maxZoom) {
    if (wheelDelta != 0.0f) {
        camera.zoom = std::clamp(camera.zoom + wheelDelta * 0.15f, minZoom, maxZoom);
    }
}

// ============================================================================
// NUEVO: Seguir objetivo (extraído de main.cpp:1534)
// ============================================================================

void RuntimeCameraManager::followTarget(Camera2D& camera, const Vector2& playerPos) {
    camera.target = playerPos;
}
