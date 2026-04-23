#pragma once
#include <raylib.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

/// A bidirectional portal connecting two scenes.
/// The player crosses from sceneA → sceneB via triggerA, or from
/// sceneB → sceneA via triggerB.  spawnInA / spawnInB define where the player
/// lands depending on which direction the portal is traversed.
struct Portal {
    std::string id;
    std::string sceneA;
    std::string sceneB;
    Rectangle   triggerA;       ///< Activation rect inside sceneA
    Rectangle   triggerB;       ///< Activation rect inside sceneB
    Vector2     spawnInA;       ///< Spawn position inside sceneA (arriving from sceneB)
    Vector2     spawnInB;       ///< Spawn position inside sceneB (arriving from sceneA)
    bool        requiresE{false};
};

/// One floor option displayed inside a FloorElevator menu.
struct FloorEntry {
    std::string scene;      ///< Target scene name (e.g. "piso3")
    Vector2     spawnPos;   ///< Spawn position inside that scene
    std::string label;      ///< Display text in the selection menu (e.g. "Piso 3")
};

/// An elevator or staircase that lets the player jump between floors 1–5.
/// The player steps into the trigger rect, presses E, and an ImGui menu appears.
struct FloorElevator {
    std::string             id;
    std::string             scene;        ///< Which scene this trigger lives in
    Rectangle               triggerRect;
    std::vector<FloorEntry> floors;       ///< Accessible floors (in menu order)
};

/// Describes a pending scene-swap (produced when the fade reaches full black).
struct TransitionRequest {
    std::string targetScene;
    Vector2     spawnPos{0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// TransitionService
// ---------------------------------------------------------------------------

/// Manages scene portals, floor-elevator menus, and the fade-in/out animation.
///
/// Usage per game frame (in order):
///   1. transitions.update(playerCollider, currentSceneName, dt);
///   2. if (transitions.needsSceneSwap()) {
///          /* unload old scene, load new scene */
///          transitions.notifySwapDone();
///      }
///   3. After EndMode2D (screen space): transitions.drawFadeOverlay(w, h);
///   4. Inside rlImGuiBegin/End:        transitions.drawFloorMenu();
///
/// For the "Presiona E" prompt: query isPromptVisible() / getPromptHint() and
/// draw the text however you prefer.
class TransitionService {
public:
    void addPortal(const Portal& portal);
    void addFloorElevator(const FloorElevator& elevator);

    /// Process triggers and advance the fade state machine. Call once per frame.
    void update(const Rectangle& playerCollider,
                const std::string& currentScene,
                float dt);

    /// True once (while swapPending_) when alpha has just reached 1.0.
    bool needsSceneSwap() const  { return swapPending_; }

    /// The transition that should be applied (load scene, reposition player).
    TransitionRequest getPendingSwap() const { return pending_; }

    /// Call after the new scene has been loaded to begin the fade-out.
    void notifySwapDone();

    bool  isFading()     const;
    float getFadeAlpha() const { return fadeAlpha_; }

    /// True when the player is inside a requiresE trigger (show prompt text).
    bool        isPromptVisible() const { return promptVisible_; }
    std::string getPromptHint()   const { return promptHint_; }

    /// Draw ImGui floor-selection window. Call inside rlImGuiBegin/End.
    void drawFloorMenu();

    /// Draw the full-screen black fade overlay (screen-space, after EndMode2D).
    void drawFadeOverlay(int screenWidth, int screenHeight) const;

    /// Read-only accessors (used for debug rendering).
    const std::vector<Portal>&        getPortals()    const { return portals_; }
    const std::vector<FloorElevator>& getElevators()  const { return elevators_; }

private:
    std::vector<Portal>        portals_;
    std::vector<FloorElevator> elevators_;

    enum class Phase { IDLE, FADING_IN, FADING_OUT } phase_{Phase::IDLE};
    float fadeAlpha_{0.0f};
    static constexpr float kFadeRate = 1.0f / 1.25f;   // 1.25 s per half

    TransitionRequest pending_{};
    bool              swapPending_{false};

    bool showFloorMenu_{false};
    int  activeElevatorIdx_{-1};

    bool        promptVisible_{false};
    std::string promptHint_;

    void beginFadeIn(const std::string& targetScene, const Vector2& spawnPos);
};
