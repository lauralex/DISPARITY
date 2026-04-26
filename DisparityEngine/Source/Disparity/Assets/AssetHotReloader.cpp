#include "Disparity/Assets/AssetHotReloader.h"

#include "Disparity/Core/FileSystem.h"

namespace Disparity
{
    void AssetHotReloader::Watch(const std::filesystem::path& path)
    {
        const std::filesystem::path resolvedPath = FileSystem::FindAssetPath(path);
        std::error_code error;
        const auto writeTime = std::filesystem::exists(resolvedPath, error)
            ? std::filesystem::last_write_time(resolvedPath, error)
            : std::filesystem::file_time_type {};

        m_knownWriteTimes[resolvedPath.string()] = writeTime;
    }

    std::vector<std::filesystem::path> AssetHotReloader::PollChanged()
    {
        std::vector<std::filesystem::path> changed;

        for (auto& [pathText, knownWriteTime] : m_knownWriteTimes)
        {
            const std::filesystem::path path(pathText);
            std::error_code error;
            if (!std::filesystem::exists(path, error))
            {
                continue;
            }

            const auto currentWriteTime = std::filesystem::last_write_time(path, error);
            if (!error && currentWriteTime != knownWriteTime)
            {
                knownWriteTime = currentWriteTime;
                changed.push_back(path);
            }
        }

        return changed;
    }

    void AssetHotReloader::Clear()
    {
        m_knownWriteTimes.clear();
    }
}
