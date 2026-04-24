#pragma once
#include "../core/graph/CampusGraph.h"
#include <string>
#include <vector>

enum class StudentType { NEW_STUDENT, REGULAR_STUDENT };

class ScenarioManager {
public:
    ScenarioManager();

    void setMobilityReduced(bool mr);
    void setStudentType(StudentType st);
    std::vector<std::string> applyProfile(const CampusGraph& graph,
                                          const std::string& origin,
                                          const std::string& destination) const;

    bool isMobilityReduced() const { return mobility_reduced_; }
    StudentType getStudentType() const { return student_type_; }

private:
    bool mobility_reduced_{false};
    StudentType student_type_{StudentType::REGULAR_STUDENT};
};
