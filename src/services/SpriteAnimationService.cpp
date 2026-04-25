#include "SpriteAnimationService.h"

#include <algorithm>

int SpriteAnimationService::directionStartFrame(int direction, int totalFrames) {
    if (totalFrames < 4) return 0;
    const int framesPerDir = std::max(1, totalFrames / 4);
    const int safeDir = std::clamp(direction, 0, 3);
    return safeDir * framesPerDir;
}

int SpriteAnimationService::directionalFrameCount(int totalFrames) {
    if (totalFrames < 4) return std::max(1, totalFrames);
    return std::max(1, totalFrames / 4);
}
