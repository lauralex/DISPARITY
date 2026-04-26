#pragma once

#include "Disparity/Scene/Scene.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Disparity
{
    struct Prefab
    {
        std::string Name = "Prefab";
        std::string MeshName = "cube";
        Transform TransformData;
        Material MaterialData;
    };

    class PrefabIO
    {
    public:
        [[nodiscard]] static bool Load(const std::filesystem::path& path, Prefab& outPrefab);
        [[nodiscard]] static bool Save(const std::filesystem::path& path, const Prefab& prefab);
        [[nodiscard]] static Prefab FromSceneObject(const NamedSceneObject& object);
        [[nodiscard]] static NamedSceneObject Instantiate(
            const Prefab& prefab,
            const std::string& instanceName,
            const DirectX::XMFLOAT3& position,
            const std::unordered_map<std::string, MeshHandle>& meshes);
    };
}
