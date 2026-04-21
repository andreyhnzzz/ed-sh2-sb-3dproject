#include "rlImGui.h"

#include "imgui.h"
#include <raylib.h>

namespace {
static bool g_ready = false;
static bool g_warned = false;

static void applyInput() {
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)GetScreenWidth(), (float)GetScreenHeight());
    float dt = GetFrameTime();
    io.DeltaTime = (dt > 0.0f) ? dt : (1.0f / 60.0f);

#if defined(IMGUI_VERSION_NUM) && (IMGUI_VERSION_NUM >= 18700)
    Vector2 mp = GetMousePosition();
    io.AddMousePosEvent(mp.x, mp.y);
    io.AddMouseButtonEvent(0, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    io.AddMouseButtonEvent(1, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    io.AddMouseButtonEvent(2, IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    io.AddMouseWheelEvent(0.0f, GetMouseWheelMove());
#else
    Vector2 mp = GetMousePosition();
    io.MousePos = ImVec2(mp.x, mp.y);
    io.MouseDown[0] = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    io.MouseDown[1] = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    io.MouseDown[2] = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    io.MouseWheel = GetMouseWheelMove();
#endif

    // Basic keyboard input (limited, but enough for demo UI)
#if defined(IMGUI_VERSION_NUM) && (IMGUI_VERSION_NUM >= 18700)
    io.AddKeyEvent(ImGuiKey_Tab, IsKeyDown(KEY_TAB));
    io.AddKeyEvent(ImGuiKey_LeftArrow, IsKeyDown(KEY_LEFT));
    io.AddKeyEvent(ImGuiKey_RightArrow, IsKeyDown(KEY_RIGHT));
    io.AddKeyEvent(ImGuiKey_UpArrow, IsKeyDown(KEY_UP));
    io.AddKeyEvent(ImGuiKey_DownArrow, IsKeyDown(KEY_DOWN));
    io.AddKeyEvent(ImGuiKey_PageUp, IsKeyDown(KEY_PAGE_UP));
    io.AddKeyEvent(ImGuiKey_PageDown, IsKeyDown(KEY_PAGE_DOWN));
    io.AddKeyEvent(ImGuiKey_Home, IsKeyDown(KEY_HOME));
    io.AddKeyEvent(ImGuiKey_End, IsKeyDown(KEY_END));
    io.AddKeyEvent(ImGuiKey_Insert, IsKeyDown(KEY_INSERT));
    io.AddKeyEvent(ImGuiKey_Delete, IsKeyDown(KEY_DELETE));
    io.AddKeyEvent(ImGuiKey_Backspace, IsKeyDown(KEY_BACKSPACE));
    io.AddKeyEvent(ImGuiKey_Space, IsKeyDown(KEY_SPACE));
    io.AddKeyEvent(ImGuiKey_Enter, IsKeyDown(KEY_ENTER));
    io.AddKeyEvent(ImGuiKey_Escape, IsKeyDown(KEY_ESCAPE));
    io.AddKeyEvent(ImGuiKey_A, IsKeyDown(KEY_A));
    io.AddKeyEvent(ImGuiKey_C, IsKeyDown(KEY_C));
    io.AddKeyEvent(ImGuiKey_V, IsKeyDown(KEY_V));
    io.AddKeyEvent(ImGuiKey_X, IsKeyDown(KEY_X));
    io.AddKeyEvent(ImGuiKey_Y, IsKeyDown(KEY_Y));
    io.AddKeyEvent(ImGuiKey_Z, IsKeyDown(KEY_Z));

    io.AddKeyEvent(ImGuiKey_ModCtrl, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    io.AddKeyEvent(ImGuiKey_ModShift, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    io.AddKeyEvent(ImGuiKey_ModAlt, IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));
    io.AddKeyEvent(ImGuiKey_ModSuper, IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER));
#else
    io.KeyCtrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    io.KeyShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    io.KeyAlt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    io.KeySuper = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

    io.KeysDown[KEY_TAB] = IsKeyDown(KEY_TAB);
    io.KeysDown[KEY_LEFT] = IsKeyDown(KEY_LEFT);
    io.KeysDown[KEY_RIGHT] = IsKeyDown(KEY_RIGHT);
    io.KeysDown[KEY_UP] = IsKeyDown(KEY_UP);
    io.KeysDown[KEY_DOWN] = IsKeyDown(KEY_DOWN);
    io.KeysDown[KEY_PAGE_UP] = IsKeyDown(KEY_PAGE_UP);
    io.KeysDown[KEY_PAGE_DOWN] = IsKeyDown(KEY_PAGE_DOWN);
    io.KeysDown[KEY_HOME] = IsKeyDown(KEY_HOME);
    io.KeysDown[KEY_END] = IsKeyDown(KEY_END);
    io.KeysDown[KEY_INSERT] = IsKeyDown(KEY_INSERT);
    io.KeysDown[KEY_DELETE] = IsKeyDown(KEY_DELETE);
    io.KeysDown[KEY_BACKSPACE] = IsKeyDown(KEY_BACKSPACE);
    io.KeysDown[KEY_SPACE] = IsKeyDown(KEY_SPACE);
    io.KeysDown[KEY_ENTER] = IsKeyDown(KEY_ENTER);
    io.KeysDown[KEY_ESCAPE] = IsKeyDown(KEY_ESCAPE);
#endif

    // Text input
    int c = 0;
    while ((c = GetCharPressed()) != 0) {
        if (c > 0 && c < 0x10000) {
            io.AddInputCharacter((unsigned int)c);
        }
    }
}
} // namespace

void rlImGuiSetup(bool darkTheme) {
    if (g_ready) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (darkTheme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }

    io.Fonts->Build();
    g_ready = true;
}

void rlImGuiBegin(void) {
    if (!g_ready) return;

    applyInput();
    ImGui::NewFrame();
}

void rlImGuiEnd(void) {
    if (!g_ready) return;

    ImGui::Render();

    if (!g_warned) {
        TraceLog(LOG_WARNING, "rlImGui minimal shim: rendering is a no-op. UI will not be visible yet.");
        g_warned = true;
    }
}

void rlImGuiShutdown(void) {
    if (!g_ready) return;
    ImGui::DestroyContext();
    g_ready = false;
}

bool rlImGuiIsReady(void) {
    return g_ready;
}
