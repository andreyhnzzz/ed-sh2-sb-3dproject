#pragma once

#include <filesystem>
#include <string>
#include <vector>

class AssetPathResolver {
public:
    static std::string findCampusJson(const char* argv0);
    static std::string resolveAssetPath(const char* argv0, const std::string& relPath);
    static std::string resolveMusicPath(const char* argv0, const std::string& musicFile);
    static std::string resolveSFXPath(const char* argv0, const std::string& sfxFile);
    static std::string findPlayerIdleSprite(const char* argv0);
    static std::string findPlayerWalkSprite(const char* argv0);

private:
    static std::string findPathCandidate(
        const char* argv0,
        const std::vector<std::filesystem::path>& baseCandidates);
};
