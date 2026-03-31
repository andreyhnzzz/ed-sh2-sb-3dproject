#pragma once
#include <glm/glm.hpp>
#include <string>

enum class DoorState { CLOSED, OPENING, OPEN, LOCKED };

class Door
{
public:
    Door(const glm::vec3& pos, const std::string& targetRoom, bool locked);

    void update(float dt);
    void tryOpen();  // called when player presses E nearby

    bool isNearPlayer(const glm::vec3& playerPos, float threshold = 3.0f) const;
    bool playerCanEnter(const glm::vec3& playerPos) const;

    DoorState         state()       const { return m_state; }
    const std::string& targetRoom() const { return m_targetRoom; }
    glm::vec3         position()    const { return m_position; }
    float             rotation()    const { return m_rotation; }

    // Model matrix for rendering (pivot at hinge = door origin)
    glm::mat4 modelMatrix() const;

private:
    glm::vec3   m_position;
    std::string m_targetRoom;
    DoorState   m_state;
    float       m_rotation;    // current Y rotation in degrees
    float       m_lerpT;
};
