#include "RuntimeUiManager.h"

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1823-1824 (drawFadeOverlay)
// ============================================================================

void RuntimeUiManager::drawFadeOverlay(const TransitionService& transitions, int screenWidth, int screenHeight) {
    transitions.drawFadeOverlay(screenWidth, screenHeight);
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1826-1837 (drawEPrompt)
// ============================================================================

void RuntimeUiManager::drawEPrompt(const TransitionService& transitions, int screenWidth, int screenHeight) {
    if (transitions.isPromptVisible()) {
        const std::string hintText = transitions.getPromptHint();
        const char* hint = hintText.c_str();
        const int hintFontSz = 22;
        const int hintW = MeasureText(hint, hintFontSz);
        const int hintX = (screenWidth - hintW) / 2;
        const int hintY = screenHeight - 60;
        DrawRectangle(hintX - 10, hintY - 8, hintW + 20, hintFontSz + 16, Color{0, 0, 0, 180});
        DrawText(hint, hintX, hintY, hintFontSz, YELLOW);
    }
}

// ============================================================================
// MIGRADO DESDE main.cpp: Líneas ~1839-1848 (drawFixedHUD)
// ============================================================================

void RuntimeUiManager::drawFixedHUD(const Vector2& playerPos, int screenWidth, int screenHeight, bool routeActive) {
    const char* coordText = TextFormat("Pos: (%.1f, %.1f)", playerPos.x, playerPos.y);
    const int coordFontSize = 20;
    const int coordPadding = 12;
    const int coordWidth = MeasureText(coordText, coordFontSize);
    const int coordX = screenWidth - coordWidth - coordPadding;
    const int coordY = coordPadding;
    DrawRectangle(coordX - 8, coordY - 6, coordWidth + 16, coordFontSize + 12, Color{0, 0, 0, 140});
    DrawText(coordText, coordX, coordY, coordFontSize, RAYWHITE);
    DrawText("M: Menu", 16, 12, 20, Color{220, 230, 255, 220});
    DrawText("TAB: POIs", 16, 34, 18, Color{255, 215, 120, 220});
    
    if (routeActive) {
        // Nota: El mensaje de "Route active" ahora se muestra en el minimapa
    }
}
