#pragma once

enum class StudentType { NEW_STUDENT, REGULAR_STUDENT };

class ScenarioManager {
public:
    ScenarioManager();

    void setMobilityReduced(bool mr);
    void setStudentType(StudentType st);

    bool isMobilityReduced() const { return mobility_reduced_; }
    StudentType getStudentType() const { return student_type_; }

private:
    bool mobility_reduced_{false};
    StudentType student_type_{StudentType::REGULAR_STUDENT};
};
