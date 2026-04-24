#pragma once

// rlImGui bridge for Raylib + Dear ImGui.
// Provides setup/frame/shutdown helpers used by src/main.cpp.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void rlImGuiSetup(bool darkTheme);
void rlImGuiBegin(void);
void rlImGuiEnd(void);
void rlImGuiShutdown(void);
bool rlImGuiIsReady(void);

#ifdef __cplusplus
}
#endif
