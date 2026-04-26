#pragma once

#include "Disparity/Math/Transform.h"
#include "Disparity/Scene/Material.h"
#include "Disparity/Scene/Mesh.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Disparity
{
    using Entity = uint32_t;

    struct NameComponent
    {
        std::string Name;
    };

    struct TransformComponent
    {
        Transform Value;
    };

    struct MeshRendererComponent
    {
        MeshHandle Mesh = 0;
        Material MaterialData;
    };

    struct RenderableEntity
    {
        Entity Id = 0;
        Transform* TransformData = nullptr;
        MeshRendererComponent* MeshRenderer = nullptr;
    };

    class Registry
    {
    public:
        [[nodiscard]] Entity CreateEntity(std::string name = {});
        void Clear();
        void DestroyEntity(Entity entity);

        TransformComponent& AddTransform(Entity entity, const Transform& transform = {});
        MeshRendererComponent& AddMeshRenderer(Entity entity, MeshHandle mesh, const Material& material = {});

        [[nodiscard]] NameComponent* GetName(Entity entity);
        [[nodiscard]] TransformComponent* GetTransform(Entity entity);
        [[nodiscard]] MeshRendererComponent* GetMeshRenderer(Entity entity);
        [[nodiscard]] std::vector<RenderableEntity> ViewRenderables();
        [[nodiscard]] size_t EntityCount() const;

    private:
        Entity m_nextEntity = 1;
        std::vector<Entity> m_entities;
        std::unordered_map<Entity, NameComponent> m_names;
        std::unordered_map<Entity, TransformComponent> m_transforms;
        std::unordered_map<Entity, MeshRendererComponent> m_meshRenderers;
    };
}
