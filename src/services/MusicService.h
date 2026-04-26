#pragma once

#include <raylib.h>

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class MusicService {
public:
    MusicService();
    ~MusicService();

    bool loadMusic(const std::string& alias, const std::string& filePath);
    void unloadAll();

    void playMusic(const std::string& alias);
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    bool isMusicPlaying() const;

    void update();
    void enableShuffle(bool enabled);
    void setMainMenuMusic(const std::string& alias);
    void addGameMusic(const std::string& alias);
    void playMainMenuMusic();
    void playGameplayMusic();

    void setVolume(float volume);

private:
    std::unordered_map<std::string, Music> loadedMusic_;
    std::string currentMusicAlias_;
    std::string mainMenuMusicAlias_;
    std::vector<std::string> gameMusicPlaylist_;
    bool shuffleEnabled_{false};
    std::mt19937 rng_;
    size_t currentTrackIndex_{0};

    bool hasLoadedMusic(const std::string& alias) const;
    std::string selectNextTrack();
};
