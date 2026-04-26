#include "AudioManager.h"

#include <algorithm>

#include <raylib.h>

AudioManager::~AudioManager() {
    shutdown();
}

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

void AudioManager::initialize() {
    if (initialized_) return;

    InitAudioDevice();
    initialized_ = true;
}

void AudioManager::shutdown() {
    if (!initialized_) return;

    CloseAudioDevice();
    initialized_ = false;
}

bool AudioManager::isInitialized() const {
    return initialized_;
}

void AudioManager::setMasterVolume(float volume) {
    const float clamped = std::clamp(volume, 0.0f, 1.0f);
    musicVolume_ = clamped;
    sfxVolume_ = clamped;
}

void AudioManager::setMusicVolume(float volume) {
    musicVolume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioManager::setSFXVolume(float volume) {
    sfxVolume_ = std::clamp(volume, 0.0f, 1.0f);
}

float AudioManager::getMusicVolume() const {
    return musicVolume_;
}

float AudioManager::getSFXVolume() const {
    return sfxVolume_;
}
