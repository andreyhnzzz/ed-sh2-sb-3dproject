#pragma once

#include "InputState.h"

class InputManager {
public:
    InputState poll(bool uiActive) const;
};
