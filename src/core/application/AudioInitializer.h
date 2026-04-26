#pragma once

#include "services/MusicService.h"
#include "services/SoundEffectService.h"

class AudioInitializer {
public:
    static void initialize();
    static void loadMusicAssets(MusicService& musicService, const char* executablePath);
    static void loadSoundEffects(SoundEffectService& soundEffectService, const char* executablePath);
};
