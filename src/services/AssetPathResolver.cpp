#include "AssetPathResolver.h"

#include <vector>

namespace fs = std::filesystem;

std::string AssetPathResolver::findPathCandidate(
    const char* argv0,
    const std::vector<fs::path>& baseCandidates) {
    std::vector<fs::path> candidates = baseCandidates;

    if (argv0 && argv0[0] != '\0') {
        const fs::path exePath = fs::absolute(argv0).parent_path();
        for (const auto& base : baseCandidates) {
            candidates.emplace_back(exePath / base);
            candidates.emplace_back(exePath / ".." / base);
            candidates.emplace_back(exePath / "../.." / base);
        }
    }

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) return candidate.string();
    }
    return "";
}

std::string AssetPathResolver::findCampusJson(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("campus.json"),
        fs::path("../campus.json"),
        fs::path("../../campus.json"),
        fs::path("../EcoCampusNav/campus.json")
    });
}

std::string AssetPathResolver::resolveAssetPath(const char* argv0, const std::string& relPath) {
    return findPathCandidate(argv0, {
        fs::path(relPath),
        fs::path("..") / relPath,
        fs::path("../..") / relPath
    });
}

std::string AssetPathResolver::resolveMusicPath(const char* argv0, const std::string& musicFile) {
    return findPathCandidate(argv0, {
        fs::path("assets/music") / musicFile,
        fs::path("../assets/music") / musicFile,
        fs::path("../../assets/music") / musicFile
    });
}

std::string AssetPathResolver::resolveSFXPath(const char* argv0, const std::string& sfxFile) {
    return findPathCandidate(argv0, {
        fs::path("assets/sound_effects") / sfxFile,
        fs::path("../assets/sound_effects") / sfxFile,
        fs::path("../../assets/sound_effects") / sfxFile
    });
}

std::string AssetPathResolver::findPlayerIdleSprite(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("assets/sprites/m_Character/junior_AnguloIdle.png"),
        fs::path("../assets/sprites/m_Character/junior_AnguloIdle.png"),
        fs::path("../../assets/sprites/m_Character/junior_AnguloIdle.png")
    });
}

std::string AssetPathResolver::findPlayerWalkSprite(const char* argv0) {
    return findPathCandidate(argv0, {
        fs::path("assets/sprites/m_Character/junior_AnguloWalk.png"),
        fs::path("../assets/sprites/m_Character/junior_AnguloWalk.png"),
        fs::path("../../assets/sprites/m_Character/junior_AnguloWalk.png")
    });
}
