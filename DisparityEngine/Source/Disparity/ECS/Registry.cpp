#include "Disparity/ECS/Registry.h"

#include <algorithm>
#include <utility>

namespace Disparity
{
    Entity Registry::CreateEntity(std::string name)
    {
        const Entity entity = m_nextEntity++;
        m_entities.push_back(entity);

        if (!name.empty())
        {
            m_names.emplace(entity, NameComponent{ std::move(name) });
        }

        return entity;
    }

    void Registry::Clear()
    {
        m_nextEntity = 1;
        m_entities.clear();
        m_names.clear();
        m_transforms.clear();
        m_meshRenderers.clear();
    }

    void Registry::DestroyEntity(Entity entity)
    {
        m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());
        m_names.erase(entity);
        m_transforms.erase(entity);
        m_meshRenderers.erase(entity);
    }

    TransformComponent& Registry::AddTransform(Entity entity, const Transform& transform)
    {
        return m_transforms.insert_or_assign(entity, TransformComponent{ transform }).first->second;
    }

    MeshRendererComponent& Registry::AddMeshRenderer(Entity entity, MeshHandle mesh, const Material& material)
    {
        return m_meshRenderers.insert_or_assign(entity, MeshRendererComponent{ mesh, material }).first->second;
    }

    NameComponent* Registry::GetName(Entity entity)
    {
        const auto found = m_names.find(entity);
        return found == m_names.end() ? nullptr : &found->second;
    }

    TransformComponent* Registry::GetTransform(Entity entity)
    {
        const auto found = m_transforms.find(entity);
        return found == m_transforms.end() ? nullptr : &found->second;
    }

    MeshRendererComponent* Registry::GetMeshRenderer(Entity entity)
    {
        const auto found = m_meshRenderers.find(entity);
        return found == m_meshRenderers.end() ? nullptr : &found->second;
    }

    std::vector<RenderableEntity> Registry::ViewRenderables()
    {
        std::vector<RenderableEntity> renderables;
        for (Entity entity : m_entities)
        {
            TransformComponent* transform = GetTransform(entity);
            MeshRendererComponent* renderer = GetMeshRenderer(entity);
            if (transform && renderer)
            {
                renderables.push_back({ entity, &transform->Value, renderer });
            }
        }

        return renderables;
    }

    size_t Registry::EntityCount() const
    {
        return m_entities.size();
    }
}
