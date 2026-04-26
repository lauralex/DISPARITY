#include "Disparity/Scripting/ScriptRunner.h"

#include "Disparity/Assets/Prefab.h"
#include "Disparity/Core/FileSystem.h"
#include "Disparity/Core/Log.h"

#include <sstream>

namespace Disparity
{
    bool ScriptRunner::RunSceneScript(
        const std::filesystem::path& path,
        Scene& scene,
        const std::unordered_map<std::string, MeshHandle>& meshes)
    {
        std::string text;
        if (!FileSystem::ReadTextFile(path, text))
        {
            return false;
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
            stream >> command;

            if (command == "spawn")
            {
                NamedSceneObject object;
                stream >> object.Name >> object.MeshName;

                const auto mesh = meshes.find(object.MeshName);
                if (mesh == meshes.end())
                {
                    Log(LogLevel::Warning, "Script references unknown mesh: " + object.MeshName);
                    continue;
                }

                object.Object.Mesh = mesh->second;
                Transform& transform = object.Object.TransformData;
                Material& material = object.Object.MaterialData;

                stream
                    >> transform.Position.x >> transform.Position.y >> transform.Position.z
                    >> transform.Scale.x >> transform.Scale.y >> transform.Scale.z
                    >> material.Albedo.x >> material.Albedo.y >> material.Albedo.z;

                if (!stream.fail())
                {
                    scene.Add(object);
                }
            }
            else if (command == "prefab")
            {
                std::string prefabPath;
                std::string instanceName;
                DirectX::XMFLOAT3 position = {};
                stream >> prefabPath >> instanceName >> position.x >> position.y >> position.z;

                Prefab prefab;
                if (!stream.fail() && PrefabIO::Load(prefabPath, prefab))
                {
                    NamedSceneObject object = PrefabIO::Instantiate(prefab, instanceName, position, meshes);
                    if (object.Object.Mesh != 0)
                    {
                        scene.Add(object);
                    }
                }
            }
        }

        return true;
    }
}
