#pragma once

class SpriteAnimationService {
public:
    static int directionStartFrame(int direction, int totalFrames);
    static int directionalFrameCount(int totalFrames);
};
