#pragma once

#include <string>

struct WindowConfig {
    int width{1280};
    int height{720};
    std::string title{"EcoCampusNav (Raylib)"};
    bool fullscreen{true};
    int targetFPS{60};
};

class WindowInitializer {
public:
    static void initialize(WindowConfig& config);
    static void cleanup();

private:
    static void setupFullscreen(WindowConfig& config);
};
