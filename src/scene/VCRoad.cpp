#include "VCRoad.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

void VCRoadManager::setCameras(const std::vector<VCRoadData>& cameras)
{
    m_cameras = cameras;
    if (!m_cameras.empty()) {
        // Start with last camera (lowest priority / fallback = index 0)
        m_activeIndex = 0;
        // Find best initial zone — prefer highest index (most specific)
        // Cameras are ordered from most general to most specific; search in
        // reverse to pick the most specific match first.
        // For now just use index 0 as default (full-map fallback).
        const auto& c = m_cameras[0];
        m_curPos    = m_fromPos    = m_toPos    = c.cam_pos;
        m_curTarget = m_fromTarget = m_toTarget = c.cam_target;
        m_curFov    = m_fromFov    = m_toFov    = c.fov;
        m_lerpT = 1.0f;
    }
}

int VCRoadManager::findZone(const glm::vec3& playerPos) const
{
    // Search from highest index to lowest so specific zones override fallback
    for (int i = static_cast<int>(m_cameras.size()) - 1; i >= 0; --i) {
        const auto& z = m_cameras[i].switch_zone;
        if (playerPos.x >= z.min_x && playerPos.x <= z.max_x &&
            playerPos.z >= z.min_z && playerPos.z <= z.max_z) {
            return i;
        }
    }
    return 0; // fallback
}

void VCRoadManager::update(const glm::vec3& playerPos, float dt)
{
    int newIndex = findZone(playerPos);
    if (newIndex != m_activeIndex) {
        m_activeIndex = newIndex;
        // Start new lerp from current position
        m_fromPos    = m_curPos;
        m_fromTarget = m_curTarget;
        m_fromFov    = m_curFov;
        m_toPos    = m_cameras[newIndex].cam_pos;
        m_toTarget = m_cameras[newIndex].cam_target;
        m_toFov    = m_cameras[newIndex].fov;
        m_lerpT = 0.0f;
    }

    if (m_lerpT < 1.0f) {
        m_lerpT += dt / m_lerpDur;
        if (m_lerpT > 1.0f) m_lerpT = 1.0f;

        float t     = m_lerpT;
        m_curPos    = glm::mix(m_fromPos,    m_toPos,    t);
        m_curTarget = glm::mix(m_fromTarget, m_toTarget, t);
        m_curFov    = glm::mix(m_fromFov,    m_toFov,    t);
    }
}

glm::mat4 VCRoadManager::getViewMatrix() const
{
    return glm::lookAt(m_curPos, m_curTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 VCRoadManager::getProjectionMatrix(float aspect) const
{
    return glm::perspective(glm::radians(m_curFov), aspect, 0.1f, 500.0f);
}
