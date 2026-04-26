#include "WindowInitializer.h"

#include "rlImGui.h"

#include <raylib.h>

void WindowInitializer::initialize(WindowConfig& config) {
    InitWindow(config.width, config.height, config.title.c_str());

    if (config.fullscreen) {
        setupFullscreen(config);
    }

    SetTargetFPS(config.targetFPS);
    rlImGuiSetup(true);
}

void WindowInitializer::setupFullscreen(WindowConfig& config) {
    const int monitor = GetCurrentMonitor();
    config.width = GetMonitorWidth(monitor);
    config.height = GetMonitorHeight(monitor);
    SetWindowSize(config.width, config.height);
    ToggleFullscreen();
}

void WindowInitializer::cleanup() {
    rlImGuiShutdown();
    CloseWindow();
}
