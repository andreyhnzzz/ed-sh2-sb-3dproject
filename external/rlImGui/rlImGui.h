#pragma once

// Minimal rlImGui shim for Raylib + Dear ImGui.
// This provides the API used by src/main.cpp without pulling a full renderer.
// Rendering is a no-op; UI logic still runs and the app compiles.

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
