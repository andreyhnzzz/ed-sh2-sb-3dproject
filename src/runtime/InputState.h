#pragma once

struct InputState {
    float moveX{0.0f};
    float moveY{0.0f};
    bool sprinting{false};
    int facingDirection{0};
    bool toggleInfoMenu{false};
    bool toggleInterestZones{false};
    float zoomWheelDelta{0.0f};
    bool uiActive{false};
};
