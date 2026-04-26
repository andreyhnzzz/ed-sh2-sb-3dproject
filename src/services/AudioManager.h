#pragma once

class AudioManager {
public:
    static AudioManager& getInstance();

    void initialize();
    void shutdown();
    bool isInitialized() const;

    void setMasterVolume(float volume);
    void setMusicVolume(float volume);
    void setSFXVolume(float volume);

    float getMusicVolume() const;
    float getSFXVolume() const;

private:
    AudioManager() = default;
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool initialized_{false};
    float musicVolume_{0.5f};
    float sfxVolume_{0.5f};
};
