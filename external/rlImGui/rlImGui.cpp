#include "rlImGui.h"

#include "imgui.h"
#include <raylib.h>
#include <rlgl.h>
#include <cstdio>
#include <cstdint>

namespace {
static bool g_ready = false;
static Texture2D g_fontTexture{};

static void updateInput() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
    const float dt = GetFrameTime();
    io.DeltaTime = dt > 0.0f ? dt : (1.0f / 60.0f);

    const Vector2 mp = GetMousePosition();
    io.AddMousePosEvent(mp.x, mp.y);
    io.AddMouseButtonEvent(0, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    io.AddMouseButtonEvent(1, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    io.AddMouseButtonEvent(2, IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    io.AddMouseWheelEvent(0.0f, GetMouseWheelMove());

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

    int c = 0;
    while ((c = GetCharPressed()) != 0) {
        if (c > 0 && c < 0x10000) {
            io.AddInputCharacter(static_cast<unsigned int>(c));
        }
    }
}

static void createFontsTexture() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    Image fontImage{};
    fontImage.data = pixels;
    fontImage.width = width;
    fontImage.height = height;
    fontImage.mipmaps = 1;
    fontImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    g_fontTexture = LoadTextureFromImage(fontImage);
    SetTextureFilter(g_fontTexture, TEXTURE_FILTER_POINT);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(g_fontTexture.id)));
}

static void renderDrawData(ImDrawData* drawData) {
    if (!drawData) return;

    const int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    const int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0) return;

    const ImVec2 clipOff = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthTest();

    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0.0f, drawData->DisplaySize.x, drawData->DisplaySize.y, 0.0f, -1.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlPushMatrix();
    rlLoadIdentity();

    rlSetBlendMode(BLEND_ALPHA);

    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdI];
            if (pcmd->UserCallback != nullptr) {
                pcmd->UserCallback(cmdList, pcmd);
                continue;
            }

            const ImVec2 clipMin(
                (pcmd->ClipRect.x - clipOff.x) * clipScale.x,
                (pcmd->ClipRect.y - clipOff.y) * clipScale.y
            );
            const ImVec2 clipMax(
                (pcmd->ClipRect.z - clipOff.x) * clipScale.x,
                (pcmd->ClipRect.w - clipOff.y) * clipScale.y
            );
            if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) continue;

            const int scissorX = static_cast<int>(clipMin.x);
            const int scissorY = static_cast<int>(clipMin.y);
            const int scissorW = static_cast<int>(clipMax.x - clipMin.x);
            const int scissorH = static_cast<int>(clipMax.y - clipMin.y);
            BeginScissorMode(scissorX, scissorY, scissorW, scissorH);

            const auto textureId = static_cast<unsigned int>(reinterpret_cast<intptr_t>(pcmd->GetTexID()));
            rlSetTexture(textureId);
            rlBegin(RL_TRIANGLES);

            const ImDrawVert* vtxBuffer = cmdList->VtxBuffer.Data + pcmd->VtxOffset;
            const ImDrawIdx* idxBuffer = cmdList->IdxBuffer.Data + pcmd->IdxOffset;
            for (unsigned int i = 0; i < pcmd->ElemCount; i++) {
                const ImDrawVert& v = vtxBuffer[idxBuffer[i]];
                const ImU32 col = v.col;
                const unsigned char r = static_cast<unsigned char>((col >> IM_COL32_R_SHIFT) & 0xFF);
                const unsigned char g = static_cast<unsigned char>((col >> IM_COL32_G_SHIFT) & 0xFF);
                const unsigned char b = static_cast<unsigned char>((col >> IM_COL32_B_SHIFT) & 0xFF);
                const unsigned char a = static_cast<unsigned char>((col >> IM_COL32_A_SHIFT) & 0xFF);
                rlColor4ub(r, g, b, a);
                rlTexCoord2f(v.uv.x, v.uv.y);
                rlVertex2f(v.pos.x, v.pos.y);
            }

            rlEnd();
            rlSetTexture(0);
            EndScissorMode();
        }
    }

    rlMatrixMode(RL_MODELVIEW);
    rlPopMatrix();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlSetTexture(0);
}
} // namespace

void rlImGuiSetup(bool darkTheme) {
    if (g_ready) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendRendererName = "rlImGui-raylib";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImFontConfig fontCfg{};
    fontCfg.OversampleH = 1;
    fontCfg.OversampleV = 1;
    fontCfg.PixelSnapH = true;
    fontCfg.SizePixels = 16.0f;
    io.Fonts->Clear();
    const char* fontPath = "assets/fonts/04B_21__.TTF";
    if (FileExists(fontPath)) {
        const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();
        if (!io.Fonts->AddFontFromFileTTF(fontPath, fontCfg.SizePixels, &fontCfg, ranges)) {
            io.Fonts->AddFontDefault(&fontCfg);
        }
    } else {
        io.Fonts->AddFontDefault(&fontCfg);
    }


    if (darkTheme) ImGui::StyleColorsDark();
    else ImGui::StyleColorsLight();

    io.Fonts->Build();
    createFontsTexture();
    g_ready = true;
}

void rlImGuiBegin(void) {
    if (!g_ready) return;
    updateInput();
    ImGui::NewFrame();
}

void rlImGuiEnd(void) {
    if (!g_ready) return;
    ImGui::Render();
    renderDrawData(ImGui::GetDrawData());
}

void rlImGuiShutdown(void) {
    if (!g_ready) return;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->SetTexID(nullptr);
    if (g_fontTexture.id != 0) {
        UnloadTexture(g_fontTexture);
        g_fontTexture = {};
    }
    ImGui::DestroyContext();
    g_ready = false;
}

bool rlImGuiIsReady(void) {
    return g_ready;
}
