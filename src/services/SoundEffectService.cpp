#include "SoundEffectService.h"

#include "AudioManager.h"

#include <algorithm>

SoundEffectService::~SoundEffectService() {
    unloadAll();
}

bool SoundEffectService::loadSound(SoundEffectType type, const std::string& filePath) {
    if (filePath.empty()) return false;

    if (sounds_.count(type) > 0) {
        UnloadSound(sounds_[type]);
    }

    Sound sound = LoadSound(filePath.c_str());
    if (sound.frameCount <= 0) return false;

    SetSoundVolume(sound, AudioManager::getInstance().getSFXVolume());
    sounds_[type] = sound;
    return true;
}

void SoundEffectService::unloadAll() {
    for (auto& [_, sound] : sounds_) {
        UnloadSound(sound);
    }
    sounds_.clear();
}

void SoundEffectService::play(SoundEffectType type) {
    const auto it = sounds_.find(type);
    if (it == sounds_.end()) return;

    SetSoundVolume(it->second, AudioManager::getInstance().getSFXVolume());
    PlaySound(it->second);
}

void SoundEffectService::setVolume(float volume) {
    const float clamped = std::clamp(volume, 0.0f, 1.0f);
    AudioManager::getInstance().setSFXVolume(clamped);
    for (auto& [_, sound] : sounds_) {
        SetSoundVolume(sound, clamped);
    }
}

float SoundEffectService::getVolume() const {
    return AudioManager::getInstance().getSFXVolume();
}
