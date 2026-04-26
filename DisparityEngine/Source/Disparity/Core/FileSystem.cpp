#include "Disparity/Core/FileSystem.h"

#include "Disparity/Core/Log.h"

#include <array>
#include <fstream>
#include <windows.h>

namespace Disparity::FileSystem
{
    std::filesystem::path GetExecutableDirectory()
    {
        std::array<wchar_t, MAX_PATH> buffer = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
        {
            return std::filesystem::current_path();
        }

        return std::filesystem::path(buffer.data()).parent_path();
    }

    std::filesystem::path FindAssetPath(const std::filesystem::path& relativePath)
    {
        if (std::filesystem::exists(relativePath))
        {
            return relativePath;
        }

        const std::array<std::filesystem::path, 2> roots = {
            std::filesystem::current_path(),
            GetExecutableDirectory()
        };

        for (const std::filesystem::path& root : roots)
        {
            std::filesystem::path cursor = root;

            for (int depth = 0; depth < 8; ++depth)
            {
                const std::filesystem::path candidate = cursor / relativePath;
                if (std::filesystem::exists(candidate))
                {
                    return candidate;
                }

                if (!cursor.has_parent_path() || cursor.parent_path() == cursor)
                {
                    break;
                }

                cursor = cursor.parent_path();
            }
        }

        return relativePath;
    }

    bool ReadTextFile(const std::filesystem::path& path, std::string& outText)
    {
        const std::filesystem::path resolvedPath = FindAssetPath(path);
        std::ifstream file(resolvedPath, std::ios::binary);
        if (!file)
        {
            Log(LogLevel::Warning, "Could not read text file: " + resolvedPath.string());
            return false;
        }

        outText.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return true;
    }

    bool ReadBinaryFile(const std::filesystem::path& path, std::vector<unsigned char>& outBytes)
    {
        const std::filesystem::path resolvedPath = FindAssetPath(path);
        std::ifstream file(resolvedPath, std::ios::binary);
        if (!file)
        {
            Log(LogLevel::Warning, "Could not read binary file: " + resolvedPath.string());
            return false;
        }

        outBytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return true;
    }

    bool WriteTextFile(const std::filesystem::path& path, const std::string& text)
    {
        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty() && !EnsureDirectory(parent))
        {
            return false;
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            Log(LogLevel::Warning, "Could not write text file: " + path.string());
            return false;
        }

        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        return true;
    }

    bool EnsureDirectory(const std::filesystem::path& path)
    {
        std::error_code error;
        if (std::filesystem::exists(path, error))
        {
            return std::filesystem::is_directory(path, error);
        }

        return std::filesystem::create_directories(path, error);
    }
}
