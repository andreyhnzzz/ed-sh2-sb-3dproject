#include "JsonGraphRepository.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

CampusGraph JsonGraphRepository::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open campus.json: " + path);

    json data = json::parse(f);
    CampusGraph g;

    for (auto& jn : data["nodes"]) {
        Node n;
        n.id = jn["id"].get<std::string>();
        n.name = jn["name"].get<std::string>();
        n.type = jn["type"].get<std::string>();
        n.x = jn["x"].get<double>();
        n.y = jn["y"].get<double>();
        n.z = jn["z"].get<double>();
        g.addNode(n);
    }

    for (auto& je : data["edges"]) {
        Edge e;
        e.from = je["from"].get<std::string>();
        e.to = je["to"].get<std::string>();
        e.base_weight = je["base_weight"].get<double>();
        e.mobility_weight = je["mobility_weight"].get<double>();
        e.blocked_for_mr = je["blocked_for_mr"].get<bool>();
        e.currently_blocked = false;
        g.addEdge(e);
    }

    return g;
}
