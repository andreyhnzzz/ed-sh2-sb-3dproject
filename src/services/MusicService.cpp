#include "MusicService.h"

#include "AudioManager.h"

#include <algorithm>

MusicService::MusicService() : rng_(std::random_device{}()) {}

MusicService::~MusicService() {
    unloadAll();
}

bool MusicService::loadMusic(const std::string& alias, const std::string& filePath) {
    if (filePath.empty()) return false;

    if (hasLoadedMusic(alias)) {
        if (currentMusicAlias_ == alias) {
            StopMusicStream(loadedMusic_[alias]);
            currentMusicAlias_.clear();
        }
        UnloadMusicStream(loadedMusic_[alias]);
    }

    Music music = LoadMusicStream(filePath.c_str());
    if (music.frameCount <= 0) return false;

    SetMusicVolume(music, AudioManager::getInstance().getMusicVolume());
    loadedMusic_[alias] = music;
    return true;
}

void MusicService::unloadAll() {
    stopMusic();
    for (auto& [_, music] : loadedMusic_) {
        UnloadMusicStream(music);
    }
    loadedMusic_.clear();
}

void MusicService::playMusic(const std::string& alias) {
    if (!hasLoadedMusic(alias)) return;

    if (!currentMusicAlias_.empty() && currentMusicAlias_ != alias && hasLoadedMusic(currentMusicAlias_)) {
        StopMusicStream(loadedMusic_[currentMusicAlias_]);
    }

    currentMusicAlias_ = alias;
    SeekMusicStream(loadedMusic_[alias], 0.0f);
    SetMusicVolume(loadedMusic_[alias], AudioManager::getInstance().getMusicVolume());
    PlayMusicStream(loadedMusic_[alias]);
}

void MusicService::stopMusic() {
    if (!currentMusicAlias_.empty() && hasLoadedMusic(currentMusicAlias_)) {
        StopMusicStream(loadedMusic_[currentMusicAlias_]);
    }
    currentMusicAlias_.clear();
}

void MusicService::pauseMusic() {
    if (!currentMusicAlias_.empty() && hasLoadedMusic(currentMusicAlias_)) {
        PauseMusicStream(loadedMusic_[currentMusicAlias_]);
    }
}

void MusicService::resumeMusic() {
    if (!currentMusicAlias_.empty() && hasLoadedMusic(currentMusicAlias_)) {
        ResumeMusicStream(loadedMusic_[currentMusicAlias_]);
    }
}

bool MusicService::isMusicPlaying() const {
    if (currentMusicAlias_.empty() || !hasLoadedMusic(currentMusicAlias_)) return false;
    return IsMusicStreamPlaying(loadedMusic_.at(currentMusicAlias_));
}

void MusicService::update() {
    if (currentMusicAlias_.empty() || !hasLoadedMusic(currentMusicAlias_)) return;

    UpdateMusicStream(loadedMusic_[currentMusicAlias_]);
    SetMusicVolume(loadedMusic_[currentMusicAlias_], AudioManager::getInstance().getMusicVolume());

    if (!shuffleEnabled_) return;

    const float timePlayed = GetMusicTimePlayed(loadedMusic_[currentMusicAlias_]);
    const float timeLength = GetMusicTimeLength(loadedMusic_[currentMusicAlias_]);
    if (timeLength > 0.0f && timePlayed >= timeLength - 0.05f) {
        const std::string nextTrack = selectNextTrack();
        if (!nextTrack.empty()) {
            playMusic(nextTrack);
        }
    }
}

void MusicService::enableShuffle(bool enabled) {
    if (shuffleEnabled_ == enabled) return;

    shuffleEnabled_ = enabled;
    if (shuffleEnabled_) {
        playGameplayMusic();
    }
}

void MusicService::setMainMenuMusic(const std::string& alias) {
    mainMenuMusicAlias_ = alias;
}

void MusicService::addGameMusic(const std::string& alias) {
    if (!hasLoadedMusic(alias)) return;

    const auto it = std::find(gameMusicPlaylist_.begin(), gameMusicPlaylist_.end(), alias);
    if (it == gameMusicPlaylist_.end()) {
        gameMusicPlaylist_.push_back(alias);
    }
}

void MusicService::playMainMenuMusic() {
    shuffleEnabled_ = false;
    if (!mainMenuMusicAlias_.empty() &&
        (currentMusicAlias_ != mainMenuMusicAlias_ || !isMusicPlaying())) {
        playMusic(mainMenuMusicAlias_);
    }
}

void MusicService::playGameplayMusic() {
    if (gameMusicPlaylist_.empty()) return;

    if (!shuffleEnabled_) {
        shuffleEnabled_ = true;
    }

    const bool playingGameplayTrack =
        std::find(gameMusicPlaylist_.begin(), gameMusicPlaylist_.end(), currentMusicAlias_) != gameMusicPlaylist_.end() &&
        isMusicPlaying();
    if (playingGameplayTrack) return;

    const std::string nextTrack = selectNextTrack();
    if (!nextTrack.empty()) {
        playMusic(nextTrack);
    }
}

void MusicService::setVolume(float volume) {
    const float clamped = std::clamp(volume, 0.0f, 1.0f);
    AudioManager::getInstance().setMusicVolume(clamped);
    if (!currentMusicAlias_.empty() && hasLoadedMusic(currentMusicAlias_)) {
        SetMusicVolume(loadedMusic_[currentMusicAlias_], clamped);
    }
}

bool MusicService::hasLoadedMusic(const std::string& alias) const {
    return loadedMusic_.count(alias) > 0;
}

std::string MusicService::selectNextTrack() {
    if (gameMusicPlaylist_.empty()) return {};
    if (gameMusicPlaylist_.size() == 1) {
        currentTrackIndex_ = 0;
        return gameMusicPlaylist_.front();
    }

    std::uniform_int_distribution<size_t> dist(0, gameMusicPlaylist_.size() - 1);
    size_t nextIndex = currentTrackIndex_;
    while (nextIndex == currentTrackIndex_) {
        nextIndex = dist(rng_);
    }

    currentTrackIndex_ = nextIndex;
    return gameMusicPlaylist_[currentTrackIndex_];
}
