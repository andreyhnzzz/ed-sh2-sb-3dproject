#pragma once
#include "Room.h"
#include <memory>
#include <string>

class SceneManager
{
public:
    static SceneManager& instance();

    bool loadFromFile(const std::string& path);
    bool loadRoom(const std::string& id);

    Room* activeRoom() { return m_activeRoom.get(); }

private:
    SceneManager() = default;
    std::vector<std::shared_ptr<Room>> m_rooms;
    std::shared_ptr<Room>              m_activeRoom;
};
