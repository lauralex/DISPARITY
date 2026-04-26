#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Disparity
{
    class AssetHotReloader
    {
    public:
        void Watch(const std::filesystem::path& path);
        [[nodiscard]] std::vector<std::filesystem::path> PollChanged();
        void Clear();

    private:
        std::unordered_map<std::string, std::filesystem::file_time_type> m_knownWriteTimes;
    };
}
