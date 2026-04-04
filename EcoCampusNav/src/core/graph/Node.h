#pragma once
#include <string>

struct Node {
    std::string id;
    std::string name;
    std::string type;
    double x{0.0}, y{0.0}, z{0.0};
};
