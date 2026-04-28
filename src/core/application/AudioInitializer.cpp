#include "AudioInitializer.h"

#include "services/AssetPathResolver.h"
#include "services/AudioManager.h"

void AudioInitializer::initialize() {
    AudioManager::getInstance().initialize();
}

void AudioInitializer::loadMusicAssets(MusicService& musicService, const char* executablePath) {
    musicService.loadMusic("main_menu",
                           AssetPathResolver::resolveMusicPath(executablePath, "main_menu.mp3"));
    musicService.loadMusic("eternal_moment",
                           AssetPathResolver::resolveMusicPath(executablePath, "eternal_moment.mp3"));
    musicService.loadMusic("quiet_day",
                           AssetPathResolver::resolveMusicPath(executablePath, "quiet_day.mp3"));
    musicService.loadMusic("the_place",
                           AssetPathResolver::resolveMusicPath(executablePath, "the_place.mp3"));
    musicService.loadMusic("your_memory",
                           AssetPathResolver::resolveMusicPath(executablePath, "your_memory.mp3"));
    musicService.loadMusic("easter_egg",
                           AssetPathResolver::resolveMusicPath(executablePath, "easter_egg.mp3"));

    musicService.setMainMenuMusic("main_menu");
    musicService.addGameMusic("eternal_moment");
    musicService.addGameMusic("quiet_day");
    musicService.addGameMusic("the_place");
    musicService.addGameMusic("your_memory");
    musicService.playMainMenuMusic();
}

void AudioInitializer::loadSoundEffects(SoundEffectService& soundEffectService,
                                        const char* executablePath) {
    soundEffectService.loadSound(
        SoundEffectType::BetweenOptions,
        AssetPathResolver::resolveSFXPath(executablePath, "between_options.mp3"));
    soundEffectService.loadSound(
        SoundEffectType::DestinationReached,
        AssetPathResolver::resolveSFXPath(executablePath, "destination_reached.mp3"));
    soundEffectService.loadSound(
        SoundEffectType::RouteFixated,
        AssetPathResolver::resolveSFXPath(executablePath, "route_fixated.mp3"));
    soundEffectService.loadSound(
        SoundEffectType::WallBump,
        AssetPathResolver::resolveSFXPath(executablePath, "wall_bump.mp3"));
    soundEffectService.loadSound(
        SoundEffectType::SelectButton,
        AssetPathResolver::resolveSFXPath(executablePath, "select_button.mp3"));
    soundEffectService.loadSound(
        SoundEffectType::ItsMe,
        AssetPathResolver::resolveSFXPath(executablePath, "its_me.mp3"));
}
