#include "Disparity/Scene/Scene.h"

#include "Disparity/Core/FileSystem.h"
#include "Disparity/Core/Log.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <string>

namespace Disparity
{
    namespace
    {
        uint64_t HashBytes(uint64_t hash, const char* data, size_t size)
        {
            constexpr uint64_t Prime = 1099511628211ull;
            for (size_t index = 0; index < size; ++index)
            {
                hash ^= static_cast<unsigned char>(data[index]);
                hash *= Prime;
            }

            return hash;
        }

        uint64_t MakeStableId(const NamedSceneObject& object, size_t salt)
        {
            uint64_t hash = 14695981039346656037ull;
            hash = HashBytes(hash, object.Name.data(), object.Name.size());
            hash = HashBytes(hash, object.MeshName.data(), object.MeshName.size());
            hash = HashBytes(hash, reinterpret_cast<const char*>(&salt), sizeof(salt));
            if (hash == 0)
            {
                hash = 1;
            }

            return hash;
        }

        bool TryParseUint64(const std::string& text, uint64_t& outValue)
        {
            if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char value) {
                return std::isdigit(value) != 0;
            }))
            {
                return false;
            }

            const char* begin = text.data();
            const char* end = begin + text.size();
            const auto result = std::from_chars(begin, end, outValue);
            return result.ec == std::errc{} && result.ptr == end;
        }
    }

    void Scene::Clear()
    {
        m_objects.clear();
    }

    void Scene::Add(NamedSceneObject object)
    {
        if (object.StableId == 0)
        {
            object.StableId = MakeStableId(object, m_objects.size());
        }

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
        output << "# DISPARITY scene v" << Scene::SchemaVersion << "\n";
        output << "schema scene " << Scene::SchemaVersion << " deterministic_ids true save_game false\n";

        for (size_t index = 0; index < m_objects.size(); ++index)
        {
            const NamedSceneObject& object = m_objects[index];
            const Transform& transform = object.Object.TransformData;
            const Material& material = object.Object.MaterialData;
            const uint64_t stableId = object.StableId == 0 ? MakeStableId(object, index) : object.StableId;
            output
                << "object " << stableId << ' ' << object.Name << ' ' << object.MeshName << ' '
                << transform.Position.x << ' ' << transform.Position.y << ' ' << transform.Position.z << ' '
                << transform.Rotation.x << ' ' << transform.Rotation.y << ' ' << transform.Rotation.z << ' '
                << transform.Scale.x << ' ' << transform.Scale.y << ' ' << transform.Scale.z << ' '
                << material.Albedo.x << ' ' << material.Albedo.y << ' ' << material.Albedo.z << ' '
                << material.Roughness << ' ' << material.Metallic << ' ' << material.Alpha << ' '
                << (material.DoubleSided ? 1 : 0) << '\n';
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
            std::string firstField;
            stream >> firstField;
            uint64_t parsedStableId = 0;
            if (TryParseUint64(firstField, parsedStableId))
            {
                object.StableId = parsedStableId;
                stream >> object.Name >> object.MeshName;
            }
            else
            {
                object.Name = firstField;
                stream >> object.MeshName;
            }

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
                int doubleSided = 0;
                if (stream >> doubleSided)
                {
                    material.DoubleSided = doubleSided != 0;
                }

                if (object.StableId == 0)
                {
                    object.StableId = MakeStableId(object, loadedObjects.size());
                }

                loadedObjects.push_back(object);
            }
        }

        m_objects = std::move(loadedObjects);
        return true;
    }
}
