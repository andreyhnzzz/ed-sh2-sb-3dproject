#include "SceneManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <QDebug>

using json = nlohmann::json;

SceneManager& SceneManager::instance()
{
    static SceneManager sm;
    return sm;
}

bool SceneManager::loadFromFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        qWarning() << "SceneManager: cannot open" << path.c_str();
        return false;
    }

    json root;
    try {
        f >> root;
    } catch (const json::exception& e) {
        qWarning() << "SceneManager JSON error:" << e.what();
        return false;
    }

    m_rooms.clear();

    for (const auto& rj : root["rooms"]) {
        auto room = std::make_shared<Room>();
        room->id   = rj["id"].get<std::string>();
        room->name = rj["name"].get<std::string>();

        // bounds
        if (rj.contains("bounds")) {
            const auto& b = rj["bounds"];
            room->min_x = b[0].get<float>();
            room->max_x = b[1].get<float>();
            room->min_z = b[2].get<float>();
            room->max_z = b[3].get<float>();
        } else {
            room->min_x = -100; room->max_x = 100;
            room->min_z = -100; room->max_z = 100;
        }

        // cameras
        std::vector<VCRoadData> cams;
        if (rj.contains("cameras")) {
            for (const auto& cj : rj["cameras"]) {
                VCRoadData vcd;
                const auto& sz = cj["switch_zone"];
                vcd.switch_zone.min_x = sz[0].get<float>();
                vcd.switch_zone.max_x = sz[1].get<float>();
                vcd.switch_zone.min_z = sz[2].get<float>();
                vcd.switch_zone.max_z = sz[3].get<float>();

                const auto& cp = cj["cam_pos"];
                vcd.cam_pos = {cp[0].get<float>(), cp[1].get<float>(), cp[2].get<float>()};

                const auto& ct = cj["cam_target"];
                vcd.cam_target = {ct[0].get<float>(), ct[1].get<float>(), ct[2].get<float>()};

                vcd.fov = cj["fov"].get<float>();
                cams.push_back(vcd);
            }
        }
        room->vcRoad.setCameras(cams);

        // doors
        if (rj.contains("doors")) {
            for (const auto& dj : rj["doors"]) {
                DoorData dd;
                dd.id          = dj["id"].get<std::string>();
                dd.target_room = dj["target_room"].get<std::string>();
                dd.locked      = dj["locked"].get<bool>();
                const auto& p  = dj["position"];
                dd.position    = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                room->doors.push_back(dd);
            }
        }

        m_rooms.push_back(room);
    }

    if (!m_rooms.empty()) {
        m_activeRoom = m_rooms[0];
        qDebug() << "SceneManager: loaded room" << m_activeRoom->id.c_str();
    }

    return true;
}

bool SceneManager::loadRoom(const std::string& id)
{
    for (auto& r : m_rooms) {
        if (r->id == id) {
            m_activeRoom = r;
            qDebug() << "→ Cargando:" << id.c_str();
            return true;
        }
    }
    qDebug() << "→ Cargando (not found, stub):" << id.c_str();
    return false;
}
