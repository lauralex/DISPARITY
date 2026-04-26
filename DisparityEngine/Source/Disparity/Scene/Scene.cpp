#include "Disparity/Scene/Scene.h"

#include "Disparity/Core/FileSystem.h"
#include "Disparity/Core/Log.h"

#include <sstream>

namespace Disparity
{
    void Scene::Clear()
    {
        m_objects.clear();
    }

    void Scene::Add(NamedSceneObject object)
    {
        m_objects.push_back(std::move(object));
    }

    const std::vector<NamedSceneObject>& Scene::GetObjects() const
    {
        return m_objects;
    }

    std::vector<NamedSceneObject>& Scene::GetObjects()
    {
        return m_objects;
    }

    size_t Scene::Count() const
    {
        return m_objects.size();
    }

    bool Scene::Save(const std::filesystem::path& path) const
    {
        std::ostringstream output;
        output << "# DISPARITY scene v1\n";

        for (const NamedSceneObject& object : m_objects)
        {
            const Transform& transform = object.Object.TransformData;
            const Material& material = object.Object.MaterialData;
            output
                << "object " << object.Name << ' ' << object.MeshName << ' '
                << transform.Position.x << ' ' << transform.Position.y << ' ' << transform.Position.z << ' '
                << transform.Rotation.x << ' ' << transform.Rotation.y << ' ' << transform.Rotation.z << ' '
                << transform.Scale.x << ' ' << transform.Scale.y << ' ' << transform.Scale.z << ' '
                << material.Albedo.x << ' ' << material.Albedo.y << ' ' << material.Albedo.z << ' '
                << material.Roughness << ' ' << material.Metallic << ' ' << material.Alpha << '\n';
        }

        return FileSystem::WriteTextFile(path, output.str());
    }

    bool Scene::Load(const std::filesystem::path& path, const std::unordered_map<std::string, MeshHandle>& meshes)
    {
        std::string text;
        if (!FileSystem::ReadTextFile(path, text))
        {
            return false;
        }

        std::vector<NamedSceneObject> loadedObjects;
        std::stringstream lines(text);
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            std::stringstream stream(line);
            std::string token;
            stream >> token;
            if (token != "object")
            {
                continue;
            }

            NamedSceneObject object;
            stream >> object.Name >> object.MeshName;

            const auto mesh = meshes.find(object.MeshName);
            if (mesh == meshes.end())
            {
                Log(LogLevel::Warning, "Scene object references unknown mesh: " + object.MeshName);
                continue;
            }

            object.Object.Mesh = mesh->second;
            Transform& transform = object.Object.TransformData;
            Material& material = object.Object.MaterialData;

            stream
                >> transform.Position.x >> transform.Position.y >> transform.Position.z
                >> transform.Rotation.x >> transform.Rotation.y >> transform.Rotation.z
                >> transform.Scale.x >> transform.Scale.y >> transform.Scale.z
                >> material.Albedo.x >> material.Albedo.y >> material.Albedo.z
                >> material.Roughness >> material.Metallic >> material.Alpha;

            if (!stream.fail())
            {
                loadedObjects.push_back(object);
            }
        }

        m_objects = std::move(loadedObjects);
        return true;
    }
}
