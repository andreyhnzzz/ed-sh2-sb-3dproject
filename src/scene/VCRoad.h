#pragma once
#include <glm/glm.hpp>
#include <vector>

struct VCRoadData {
    struct {
        float min_x, max_x, min_z, max_z;
    } switch_zone;
    glm::vec3 cam_pos;
    glm::vec3 cam_target;
    float fov;
};

class VCRoadManager
{
public:
    void setCameras(const std::vector<VCRoadData>& cameras);

    // Call each frame; detects zone change and starts lerp
    void update(const glm::vec3& playerPos, float dt);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;

    glm::vec3 currentCamPos()    const { return m_curPos;    }
    glm::vec3 currentCamTarget() const { return m_curTarget; }

private:
    std::vector<VCRoadData> m_cameras;

    int m_activeIndex  = 0;

    glm::vec3 m_curPos;
    glm::vec3 m_curTarget;
    float     m_curFov = 55.0f;

    glm::vec3 m_fromPos;
    glm::vec3 m_fromTarget;
    float     m_fromFov = 55.0f;

    glm::vec3 m_toPos;
    glm::vec3 m_toTarget;
    float     m_toFov = 55.0f;

    float m_lerpT    = 1.0f;  // 1 = no transition active
    float m_lerpDur  = 0.4f;

    int findZone(const glm::vec3& playerPos) const;
};
