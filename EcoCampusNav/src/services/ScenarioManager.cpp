#include "ScenarioManager.h"

ScenarioManager::ScenarioManager() = default;

void ScenarioManager::setMobilityReduced(bool mr) { mobility_reduced_ = mr; }
void ScenarioManager::setStudentType(StudentType st) { student_type_ = st; }
