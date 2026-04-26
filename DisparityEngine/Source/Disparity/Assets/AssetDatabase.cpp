#include "Disparity/Assets/AssetDatabase.h"

#include "Disparity/Assets/MaterialAsset.h"
#include "Disparity/Assets/SimpleJson.h"
#include "Disparity/Core/FileSystem.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace Disparity
{
    namespace
    {
        std::string LowerExtension(const std::filesystem::path& path)
        {
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
            return extension;
        }

        bool IsDataUri(const std::string& uri)
        {
            return uri.rfind("data:", 0) == 0;
        }

        std::filesystem::path ResolveSiblingPath(const std::filesystem::path& owner, const std::string& uri)
        {
            if (uri.empty() || IsDataUri(uri))
            {
                return {};
            }

            std::filesystem::path dependency(uri);
            if (dependency.is_absolute())
            {
                return dependency.lexically_normal();
            }

            return (owner.parent_path() / dependency).lexically_normal();
        }

        void AddDependency(std::vector<std::filesystem::path>& dependencies, const std::filesystem::path& dependency)
        {
            if (dependency.empty())
            {
                return;
            }

            const std::filesystem::path normalized = dependency.lexically_normal();
            const auto found = std::find(dependencies.begin(), dependencies.end(), normalized);
            if (found == dependencies.end())
            {
                dependencies.push_back(normalized);
            }
        }

        void CollectGltfUriDependencies(const JsonValue* values, const std::filesystem::path& gltfPath, std::vector<std::filesystem::path>& dependencies)
        {
            if (!values || !values->IsArray())
            {
                return;
            }

            for (const JsonValue& value : values->Array)
            {
                if (!value.IsObject())
                {
                    continue;
                }

                const JsonValue* uri = value.Find("uri");
                if (uri && uri->IsString())
                {
                    AddDependency(dependencies, ResolveSiblingPath(gltfPath, uri->String));
                }
            }
        }

        void CollectGltfDependencies(const std::filesystem::path& path, std::vector<std::filesystem::path>& dependencies)
        {
            if (LowerExtension(path) == ".glb")
            {
                return;
            }

            std::string text;
            if (!FileSystem::ReadTextFile(path, text))
            {
                return;
            }

            JsonValue root;
            if (!SimpleJson::Parse(text, root) || !root.IsObject())
            {
                return;
            }

            CollectGltfUriDependencies(root.Find("buffers"), path, dependencies);
            CollectGltfUriDependencies(root.Find("images"), path, dependencies);
        }

        void CollectMaterialDependencies(const std::filesystem::path& path, std::vector<std::filesystem::path>& dependencies)
        {
            MaterialAsset asset;
            if (!MaterialAssetIO::Load(path, asset))
            {
                return;
            }

            AddDependency(dependencies, asset.BaseColorTexturePath);
        }

        void CollectScriptDependencies(const std::filesystem::path& path, std::vector<std::filesystem::path>& dependencies)
        {
            std::string text;
            if (!FileSystem::ReadTextFile(path, text))
            {
                return;
            }

            std::stringstream lines(text);
            std::string line;
            while (std::getline(lines, line))
            {
                if (line.empty() || line[0] == '#')
                {
                    continue;
                }

                std::stringstream stream(line);
                std::string command;
                std::string dependency;
                stream >> command >> dependency;
                if (command == "prefab")
                {
                    AddDependency(dependencies, dependency);
                }
            }
        }
    }

    bool AssetDatabase::Scan(const std::filesystem::path& root)
    {
        m_root = root;
        m_records.clear();

        const std::filesystem::path resolvedRoot = FileSystem::FindAssetPath(root);
        std::error_code error;
        if (!std::filesystem::exists(resolvedRoot, error) || !std::filesystem::is_directory(resolvedRoot, error))
        {
            return false;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(resolvedRoot, error))
        {
            if (error)
            {
                break;
            }

            if (!entry.is_regular_file(error))
            {
                continue;
            }

            const std::filesystem::path absolutePath = entry.path();
            AssetRecord record;
            record.Path = MakeDisplayPath(absolutePath);
            record.Kind = KindFromPath(absolutePath);
            record.SizeBytes = entry.file_size(error);
            record.LastWriteTime = entry.last_write_time(error);
            record.CookedPath = MakeCookedPath(record.Path);

            if (record.Kind == AssetKind::Mesh)
            {
                CollectGltfDependencies(absolutePath, record.Dependencies);
            }
            else if (record.Kind == AssetKind::Material)
            {
                CollectMaterialDependencies(absolutePath, record.Dependencies);
            }
            else if (record.Kind == AssetKind::Script)
            {
                CollectScriptDependencies(absolutePath, record.Dependencies);
            }

            for (std::filesystem::path& dependency : record.Dependencies)
            {
                dependency = MakeDisplayPath(dependency);
            }

            record.Dirty = IsDirty(record, absolutePath);
            m_records.push_back(std::move(record));
        }

        std::sort(m_records.begin(), m_records.end(), [](const AssetRecord& a, const AssetRecord& b) {
            if (a.Kind != b.Kind)
            {
                return static_cast<int>(a.Kind) < static_cast<int>(b.Kind);
            }

            return a.Path.string() < b.Path.string();
        });
        return true;
    }

    const std::vector<AssetRecord>& AssetDatabase::GetRecords() const
    {
        return m_records;
    }

    std::map<AssetKind, size_t> AssetDatabase::CountByKind() const
    {
        std::map<AssetKind, size_t> counts;
        for (const AssetRecord& record : m_records)
        {
            ++counts[record.Kind];
        }
        return counts;
    }

    std::optional<AssetRecord> AssetDatabase::Find(const std::filesystem::path& path) const
    {
        const std::filesystem::path displayPath = MakeDisplayPath(path);
        const auto found = std::find_if(m_records.begin(), m_records.end(), [&displayPath](const AssetRecord& record) {
            return record.Path == displayPath;
        });
        if (found == m_records.end())
        {
            return std::nullopt;
        }

        return *found;
    }

    size_t AssetDatabase::DirtyCount() const
    {
        return static_cast<size_t>(std::count_if(m_records.begin(), m_records.end(), [](const AssetRecord& record) {
            return record.Dirty;
        }));
    }

    AssetKind AssetDatabase::KindFromPath(const std::filesystem::path& path)
    {
        const std::string extension = LowerExtension(path);
        if (extension == ".dscene")
        {
            return AssetKind::Scene;
        }
        if (extension == ".dscript")
        {
            return AssetKind::Script;
        }
        if (extension == ".dprefab")
        {
            return AssetKind::Prefab;
        }
        if (extension == ".gltf" || extension == ".glb")
        {
            return AssetKind::Mesh;
        }
        if (extension == ".dmat")
        {
            return AssetKind::Material;
        }
        if (extension == ".hlsl")
        {
            return AssetKind::Shader;
        }
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga")
        {
            return AssetKind::Texture;
        }
        if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
        {
            return AssetKind::Audio;
        }

        return AssetKind::Unknown;
    }

    const char* AssetDatabase::KindToString(AssetKind kind)
    {
        switch (kind)
        {
        case AssetKind::Scene: return "Scene";
        case AssetKind::Script: return "Script";
        case AssetKind::Prefab: return "Prefab";
        case AssetKind::Mesh: return "Mesh";
        case AssetKind::Material: return "Material";
        case AssetKind::Shader: return "Shader";
        case AssetKind::Texture: return "Texture";
        case AssetKind::Audio: return "Audio";
        default: return "Unknown";
        }
    }

    std::filesystem::path AssetDatabase::MakeDisplayPath(const std::filesystem::path& path) const
    {
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(path, std::filesystem::current_path(), error);
        if (!error && !relative.empty())
        {
            return relative.lexically_normal();
        }

        return path.lexically_normal();
    }

    std::filesystem::path AssetDatabase::MakeCookedPath(const std::filesystem::path& path) const
    {
        std::filesystem::path cooked = std::filesystem::path("Saved") / "CookedAssets" / path;
        cooked += ".cooked";
        return cooked.lexically_normal();
    }

    bool AssetDatabase::IsDirty(const AssetRecord& record, const std::filesystem::path& absolutePath) const
    {
        std::error_code error;
        if (!std::filesystem::exists(record.CookedPath, error))
        {
            return true;
        }

        const std::filesystem::file_time_type cookedTime = std::filesystem::last_write_time(record.CookedPath, error);
        if (error)
        {
            return true;
        }

        const std::filesystem::file_time_type sourceTime = std::filesystem::last_write_time(absolutePath, error);
        if (!error && sourceTime > cookedTime)
        {
            return true;
        }

        for (const std::filesystem::path& dependency : record.Dependencies)
        {
            const std::filesystem::path resolved = FileSystem::FindAssetPath(dependency);
            if (std::filesystem::exists(resolved, error))
            {
                const std::filesystem::file_time_type dependencyTime = std::filesystem::last_write_time(resolved, error);
                if (!error && dependencyTime > cookedTime)
                {
                    return true;
                }
            }
        }

        return false;
    }
}
