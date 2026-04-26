#include "DataManager.h"

#include "../services/TmjLoader.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

Vector2 resolveAnchorPoint(
    const json& definition,
    const std::unordered_map<std::string, std::unordered_map<std::string, Vector2>>& spawnCache,
    const std::string& defaultScene,
    const std::string& defaultSpawnId) {
    if (definition.contains("x") && definition.contains("y")) {
        return Vector2{
            definition["x"].get<float>(),
            definition["y"].get<float>()
        };
    }

    const std::string scene = toLower(definition.value("scene", defaultScene));
    const std::string spawnId = definition.value("spawn_id", defaultSpawnId);
    const auto sceneIt = spawnCache.find(scene);
    if (sceneIt == spawnCache.end()) {
        throw std::runtime_error("No TMJ data for scene: " + scene);
    }
    const auto spawnIt = sceneIt->second.find(spawnId);
    if (spawnIt == sceneIt->second.end()) {
        throw std::runtime_error("Spawn not found: " + scene + "::" + spawnId);
    }
    return spawnIt->second;
}

double euclideanMeters(const Vector2& from, const Vector2& to, double pixelsToMeters) {
    const double dx = static_cast<double>(to.x - from.x);
    const double dy = static_cast<double>(to.y - from.y);
    return std::sqrt(dx * dx + dy * dy) * pixelsToMeters;
}

bool isStairType(const std::string& type) {
    const std::string lowered = toLower(type);
    return lowered.find("escalera") != std::string::npos ||
           lowered.find("escal") != std::string::npos ||
           lowered.find("stair") != std::string::npos;
}

std::string sanitizeForId(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        } else if (ch == ' ' || ch == '_' || ch == '-') {
            if (out.empty() || out.back() == '_') continue;
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "zone" : out;
}

bool computeZoneCenter(const json& zoneDef, Vector2& outCenter) {
    if (!zoneDef.contains("rects") || !zoneDef["rects"].is_array() || zoneDef["rects"].empty()) {
        return false;
    }

    double accX = 0.0;
    double accY = 0.0;
    int count = 0;
    for (const auto& rect : zoneDef["rects"]) {
        if (!rect.contains("x") || !rect.contains("y") ||
            !rect.contains("w") || !rect.contains("h")) {
            continue;
        }
        const double x = rect["x"].get<double>();
        const double y = rect["y"].get<double>();
        const double w = rect["w"].get<double>();
        const double h = rect["h"].get<double>();
        accX += x + (w * 0.5);
        accY += y + (h * 0.5);
        ++count;
    }

    if (count == 0) return false;
    outCenter = {
        static_cast<float>(accX / static_cast<double>(count)),
        static_cast<float>(accY / static_cast<double>(count))
    };
    return true;
}
} // namespace

std::unordered_map<std::string, std::unordered_map<std::string, Vector2>>
DataManager::loadSpawnCache(
    const std::unordered_map<std::string, std::string>& sceneToTmjPath) const {
    std::unordered_map<std::string, std::unordered_map<std::string, Vector2>> cache;
    for (const auto& [sceneId, tmjPath] : sceneToTmjPath) {
        cache[toLower(sceneId)] = loadSpawnsFromTmj(tmjPath);
    }
    return cache;
}

CampusGraph DataManager::loadCampusGraph(
    const std::string& configPath,
    const std::unordered_map<std::string, std::string>& sceneToTmjPath,
    const std::string& interestZonesPath,
    double pixelsToMeters) const {
    std::ifstream input(configPath);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open campus config: " + configPath);
    }

    json data = json::parse(input);
    if (!data.contains("nodes") || !data["nodes"].is_array() ||
        !data.contains("edges") || !data["edges"].is_array()) {
        throw std::runtime_error("Campus config must contain arrays: nodes, edges");
    }

    const auto spawnCache = loadSpawnCache(sceneToTmjPath);
    CampusGraph graph;
    std::unordered_map<std::string, std::string> sceneToMainNode;
    for (const auto& jn : data["nodes"]) {
        Node node;
        node.id = toLower(jn.at("id").get<std::string>());
        node.name = jn.value("name", node.id);
        node.type = jn.value("type", "");

        const std::string scene = toLower(jn.value("scene", ""));
        const std::string spawnId = jn.value("spawn_id", "");
        const Vector2 pos = resolveAnchorPoint(jn, spawnCache, scene, spawnId);
        node.x = pos.x;
        node.y = pos.y;
        graph.addNode(node);
        if (!scene.empty() && !sceneToMainNode.count(scene)) {
            sceneToMainNode[scene] = node.id;
        }
    }

    for (const auto& je : data["edges"]) {
        const std::string from = toLower(je.at("from").get<std::string>());
        const std::string to = toLower(je.at("to").get<std::string>());
        if (!graph.hasNode(from) || !graph.hasNode(to)) {
            throw std::runtime_error("Edge references unknown node: " + from + " -> " + to);
        }

        const json defaultFromAnchor = json{
            {"x", graph.getNode(from).x},
            {"y", graph.getNode(from).y}
        };
        const json defaultToAnchor = json{
            {"x", graph.getNode(to).x},
            {"y", graph.getNode(to).y}
        };
        const Vector2 fromAnchor = je.contains("from_anchor")
            ? resolveAnchorPoint(je["from_anchor"], spawnCache, "", "")
            : resolveAnchorPoint(defaultFromAnchor, spawnCache, "", "");
        const Vector2 toAnchor = je.contains("to_anchor")
            ? resolveAnchorPoint(je["to_anchor"], spawnCache, "", "")
            : resolveAnchorPoint(defaultToAnchor, spawnCache, "", "");

        const std::string type = je.value("type", "Portal");
        const double distanceMeters = euclideanMeters(fromAnchor, toAnchor, pixelsToMeters);

        Edge edge;
        edge.id = je.value("id", from + "_" + to + "_" + toLower(type));
        edge.from = from;
        edge.to = to;
        edge.type = type;
        edge.base_weight = distanceMeters;
        edge.blocked_for_mr = isStairType(type);
        edge.mobility_weight = edge.blocked_for_mr
            ? std::numeric_limits<double>::infinity()
            : distanceMeters;
        graph.addEdge(edge);
    }

    std::string resolvedInterestZonesPath = interestZonesPath;
    if (resolvedInterestZonesPath.empty()) {
        const fs::path configDir = fs::path(configPath).parent_path();
        const std::vector<fs::path> candidates = {
            configDir / "assets" / "interest_zones.json",
            configDir / ".." / "assets" / "interest_zones.json",
            fs::path("assets/interest_zones.json")
        };
        for (const auto& candidate : candidates) {
            if (fs::exists(candidate)) {
                resolvedInterestZonesPath = candidate.string();
                break;
            }
        }
    }

    if (!resolvedInterestZonesPath.empty()) {
        std::ifstream zonesInput(resolvedInterestZonesPath);
        if (zonesInput.is_open()) {
            json zonesData = json::parse(zonesInput, nullptr, false);
            if (!zonesData.is_discarded() &&
                zonesData.contains("scenes") &&
                zonesData["scenes"].is_object()) {
                for (auto it = zonesData["scenes"].begin(); it != zonesData["scenes"].end(); ++it) {
                    const std::string sceneId = toLower(it.key());
                    const auto sceneNodeIt = sceneToMainNode.find(sceneId);
                    if (sceneNodeIt == sceneToMainNode.end()) continue;
                    if (!it.value().is_array()) continue;

                    const std::string& anchorNodeId = sceneNodeIt->second;
                    const Node& anchorNode = graph.getNode(anchorNodeId);
                    int zoneIndex = 0;
                    for (const auto& zone : it.value()) {
                        if (!zone.is_object()) continue;
                        Vector2 center{};
                        if (!computeZoneCenter(zone, center)) continue;

                        const std::string zoneName = zone.value("name", "POI");
                        const std::string zoneSlug = sanitizeForId(zoneName);
                        std::string poiNodeId = "poi_" + sceneId + "_" + zoneSlug;
                        if (zoneIndex > 0) {
                            poiNodeId += "_" + std::to_string(zoneIndex + 1);
                        }
                        ++zoneIndex;

                        // Avoid accidental id collisions with pre-existing nodes.
                        while (graph.hasNode(poiNodeId)) {
                            poiNodeId += "_x";
                        }

                        Node poiNode;
                        poiNode.id = poiNodeId;
                        poiNode.name = zoneName;
                        poiNode.type = "POI";
                        poiNode.x = center.x;
                        poiNode.y = center.y;
                        graph.addNode(poiNode);

                        const Vector2 anchorPos{
                            static_cast<float>(anchorNode.x),
                            static_cast<float>(anchorNode.y)
                        };
                        Edge poiEdge;
                        poiEdge.id = "edge_" + anchorNodeId + "_" + poiNodeId;
                        poiEdge.from = anchorNodeId;
                        poiEdge.to = poiNodeId;
                        poiEdge.type = "POI";
                        poiEdge.base_weight = euclideanMeters(anchorPos, center, pixelsToMeters);
                        poiEdge.mobility_weight = poiEdge.base_weight;
                        poiEdge.blocked_for_mr = false;
                        graph.addEdge(poiEdge);
                    }
                }
            }
        }
    }

    return graph;
}

void DataManager::exportResolvedGraph(const CampusGraph& graph, const std::string& outputPath) const {
    json out;
    out["nodes"] = json::array();
    out["edges"] = json::array();

    for (const auto& id : graph.nodeIds()) {
        const auto& node = graph.getNode(id);
        out["nodes"].push_back({
            {"id", node.id},
            {"name", node.name},
            {"type", node.type},
            {"x", node.x},
            {"y", node.y},
            {"z", node.z}
        });
    }

    for (const auto& id : graph.nodeIds()) {
        for (const auto& edge : graph.edgesFrom(id)) {
            if (edge.from > edge.to) continue;
            out["edges"].push_back({
                {"id", edge.id},
                {"from", edge.from},
                {"to", edge.to},
                {"type", edge.type},
                {"base_weight", edge.base_weight},
                {"mobility_weight", edge.mobility_weight},
                {"blocked_for_mr", edge.blocked_for_mr}
            });
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot write resolved graph: " + outputPath);
    }
    output << out.dump(2);
}
