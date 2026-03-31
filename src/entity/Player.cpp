#include "Player.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

Player::Player()
    : m_position(0.0f, 0.9f, -10.0f)
    , m_yaw(0.0f)
    , m_speed(5.0f)
{}

void Player::update(float dt, bool forward, bool back, bool left, bool right)
{
    glm::vec3 dir(0.0f);

    // Movement in world space (not camera-relative, like SH1/SH2)
    if (forward) dir.z += 1.0f;
    if (back)    dir.z -= 1.0f;
    if (left)    dir.x -= 1.0f;
    if (right)   dir.x += 1.0f;

    if (glm::length(dir) > 0.001f) {
        dir = glm::normalize(dir);
        m_position += dir * m_speed * dt;

        // Visual rotation towards movement direction (lerp)
        float targetYaw = std::atan2(dir.x, dir.z);
        // Lerp angle
        float diff = targetYaw - m_yaw;
        // Wrap to [-π, π]
        while (diff >  glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
        while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
        m_yaw += diff * std::min(1.0f, 10.0f * dt);
    }

    // Clamp position
    m_position.x = std::clamp(m_position.x, -95.0f, 95.0f);
    m_position.z = std::clamp(m_position.z, -95.0f, 95.0f);
}

glm::mat4 Player::modelMatrix() const
{
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, m_position);
    m = glm::rotate(m, m_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::scale(m, glm::vec3(0.5f, 1.8f, 0.5f));
    return m;
}
