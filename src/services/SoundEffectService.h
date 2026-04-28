#pragma once

#include <raylib.h>

#include <string>
#include <unordered_map>

enum class SoundEffectType {
    BetweenOptions,
    DestinationReached,
    RouteFixated,
    WallBump,
    SelectButton,
    ItsMe
};

class SoundEffectService {
public:
    SoundEffectService() = default;
    ~SoundEffectService();

    bool loadSound(SoundEffectType type, const std::string& filePath);
    void unloadAll();
    void play(SoundEffectType type);
    void setVolume(float volume);
    float getVolume() const;

private:
    std::unordered_map<SoundEffectType, Sound> sounds_;
};
