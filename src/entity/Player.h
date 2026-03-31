#pragma once
#include <glm/glm.hpp>
#include <string>

class Player
{
public:
    Player();

    void update(float dt, bool moveForward, bool moveBack,
                bool moveLeft, bool moveRight);

    glm::vec3 position() const { return m_position; }
    float     yaw()      const { return m_yaw; }

    // Model matrix for rendering
    glm::mat4 modelMatrix() const;

private:
    glm::vec3 m_position;
    float     m_yaw;       // visual rotation in radians
    float     m_speed;
};
