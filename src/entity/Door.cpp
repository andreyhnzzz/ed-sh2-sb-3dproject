#include "Door.h"
#include "core/EventBus.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Door::Door(const glm::vec3& pos, const std::string& targetRoom, bool locked)
    : m_position(pos)
    , m_targetRoom(targetRoom)
    , m_state(locked ? DoorState::LOCKED : DoorState::CLOSED)
    , m_rotation(0.0f)
    , m_lerpT(0.0f)
{}

void Door::update(float dt)
{
    if (m_state == DoorState::OPENING) {
        float dur = 0.4f;
        m_lerpT += dt / dur;
        if (m_lerpT >= 1.0f) {
            m_lerpT    = 1.0f;
            m_rotation = 90.0f;
            m_state    = DoorState::OPEN;
        } else {
            m_rotation = m_lerpT * 90.0f;
        }
    }
}

void Door::tryOpen()
{
    if (m_state == DoorState::CLOSED) {
        m_state  = DoorState::OPENING;
        m_lerpT  = 0.0f;
        EventBus::instance().publish("door_opened");
    } else if (m_state == DoorState::LOCKED) {
        EventBus::instance().publish("door_locked");
    }
}

bool Door::isNearPlayer(const glm::vec3& playerPos, float threshold) const
{
    float dx = playerPos.x - m_position.x;
    float dz = playerPos.z - m_position.z;
    return (dx * dx + dz * dz) < (threshold * threshold);
}

bool Door::playerCanEnter(const glm::vec3& playerPos) const
{
    if (m_state != DoorState::OPEN) return false;
    float dx = playerPos.x - m_position.x;
    float dz = playerPos.z - m_position.z;
    return (dx * dx + dz * dz) < (0.8f * 0.8f);
}

glm::mat4 Door::modelMatrix() const
{
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, m_position + glm::vec3(0.0f, 1.0f, 0.0f)); // center at y=1
    m = glm::rotate(m, glm::radians(m_rotation), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::scale(m, glm::vec3(1.0f, 2.0f, 0.1f));
    return m;
}
