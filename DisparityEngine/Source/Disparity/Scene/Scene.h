#pragma once

#include "Disparity/Scene/SceneObject.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Disparity
{
    struct NamedSceneObject
    {
        uint64_t StableId = 0;
        std::string Name;
        std::string MeshName;
        SceneObject Object;
    };

    class Scene
    {
    public:
        static constexpr uint32_t SchemaVersion = 3;

        void Clear();
        void Add(NamedSceneObject object);

        [[nodiscard]] const std::vector<NamedSceneObject>& GetObjects() const;
        [[nodiscard]] std::vector<NamedSceneObject>& GetObjects();
        [[nodiscard]] size_t Count() const;

        [[nodiscard]] bool Save(const std::filesystem::path& path) const;
        [[nodiscard]] bool Load(const std::filesystem::path& path, const std::unordered_map<std::string, MeshHandle>& meshes);

    private:
        std::vector<NamedSceneObject> m_objects;
    };
}
