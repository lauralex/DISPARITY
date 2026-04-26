#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Disparity::FileSystem
{
    [[nodiscard]] std::filesystem::path GetExecutableDirectory();
    [[nodiscard]] std::filesystem::path FindAssetPath(const std::filesystem::path& relativePath);

    [[nodiscard]] bool ReadTextFile(const std::filesystem::path& path, std::string& outText);
    [[nodiscard]] bool ReadBinaryFile(const std::filesystem::path& path, std::vector<unsigned char>& outBytes);
    [[nodiscard]] bool WriteTextFile(const std::filesystem::path& path, const std::string& text);
    [[nodiscard]] bool EnsureDirectory(const std::filesystem::path& path);
}
