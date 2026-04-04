#pragma once
#include <string>

struct Edge {
    std::string from;
    std::string to;
    double base_weight{1.0};
    double mobility_weight{1.0};
    bool blocked_for_mr{false};
    bool currently_blocked{false};
};
