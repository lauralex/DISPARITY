#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <vector>

namespace Disparity
{
    enum class AssetKind
    {
        Unknown,
        Scene,
        Script,
        Prefab,
        Mesh,
        Material,
        Shader,
        Texture,
        Audio,
        ImportSettings
    };

    struct AssetRecord
    {
        std::filesystem::path Path;
        AssetKind Kind = AssetKind::Unknown;
        uintmax_t SizeBytes = 0;
        std::filesystem::file_time_type LastWriteTime = {};
        std::vector<std::filesystem::path> Dependencies;
        std::filesystem::path ImportSettingsPath;
        bool HasImportSettings = false;
        std::filesystem::path CookedPath;
        bool Dirty = true;
    };

    class AssetDatabase
    {
    public:
        [[nodiscard]] bool Scan(const std::filesystem::path& root = "Assets");

        [[nodiscard]] const std::vector<AssetRecord>& GetRecords() const;
        [[nodiscard]] std::map<AssetKind, size_t> CountByKind() const;
        [[nodiscard]] std::optional<AssetRecord> Find(const std::filesystem::path& path) const;
        [[nodiscard]] std::map<std::filesystem::path, std::vector<std::filesystem::path>> BuildDependencyGraph() const;
        [[nodiscard]] std::vector<std::filesystem::path> FindDependents(const std::filesystem::path& path) const;
        [[nodiscard]] size_t DirtyCount() const;
        [[nodiscard]] size_t CookDirtyAssets() const;

        [[nodiscard]] static AssetKind KindFromPath(const std::filesystem::path& path);
        [[nodiscard]] static const char* KindToString(AssetKind kind);

    private:
        [[nodiscard]] std::filesystem::path MakeDisplayPath(const std::filesystem::path& path) const;
        [[nodiscard]] std::filesystem::path MakeCookedPath(const std::filesystem::path& path) const;
        [[nodiscard]] std::filesystem::path MakeImportSettingsPath(const std::filesystem::path& path) const;
        [[nodiscard]] bool IsDirty(const AssetRecord& record, const std::filesystem::path& absolutePath) const;

        std::filesystem::path m_root = "Assets";
        std::vector<AssetRecord> m_records;
    };
}
