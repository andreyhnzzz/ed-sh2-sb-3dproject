#include "ScenarioManager.h"
#include <algorithm>
#include <cctype>

ScenarioManager::ScenarioManager() = default;

void ScenarioManager::setMobilityReduced(bool mr) { mobility_reduced_ = mr; }
void ScenarioManager::setStudentType(StudentType st) { student_type_ = st; }

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static bool containsAll(const std::string& haystack, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (haystack.find(needle) == std::string::npos) return false;
    }
    return true;
}

std::vector<std::string> ScenarioManager::applyProfile(const CampusGraph& graph,
                                                       const std::string& origin,
                                                       const std::string& destination) const {
    std::vector<std::string> waypoints;
    if (origin.empty() || destination.empty()) return waypoints;
    waypoints.push_back(origin);

    if (student_type_ == StudentType::NEW_STUDENT) {
        std::string bibliotecaId;
        std::string sodaId;
        for (const auto& id : graph.nodeIds()) {
            try {
                const auto& n = graph.getNode(id);
                const std::string lowerName = toLower(n.name);
                const std::string lowerType = toLower(n.type);
                const std::string joined = lowerName + " " + lowerType;
                if (bibliotecaId.empty() &&
                    (joined.find("bibli") != std::string::npos || joined.find("biblio") != std::string::npos)) {
                    bibliotecaId = id;
                }
                if (sodaId.empty() &&
                    (joined.find("soda") != std::string::npos ||
                     joined.find("comedor") != std::string::npos ||
                     containsAll(joined, {"cafeter"}))) {
                    sodaId = id;
                }
            } catch (...) {}
        }

        if (!bibliotecaId.empty() && bibliotecaId != origin && bibliotecaId != destination) {
            waypoints.push_back(bibliotecaId);
        }
        if (!sodaId.empty() && sodaId != origin && sodaId != destination &&
            (waypoints.empty() || waypoints.back() != sodaId)) {
            waypoints.push_back(sodaId);
        }
    }

    if (waypoints.empty() || waypoints.back() != destination) {
        waypoints.push_back(destination);
    }
    return waypoints;
}
