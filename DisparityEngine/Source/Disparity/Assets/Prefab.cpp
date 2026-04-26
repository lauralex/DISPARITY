#include "Disparity/Assets/Prefab.h"

#include "Disparity/Core/FileSystem.h"

#include <sstream>

namespace Disparity
{
    namespace
    {
        DirectX::XMFLOAT3 ParseFloat3(const std::string& value, DirectX::XMFLOAT3 fallback)
        {
            std::stringstream stream(value);
            char comma = 0;
            DirectX::XMFLOAT3 result = fallback;
            stream >> result.x >> comma >> result.y >> comma >> result.z;
            return stream.fail() ? fallback : result;
        }
    }

    bool PrefabIO::Load(const std::filesystem::path& path, Prefab& outPrefab)
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

            const size_t equals = line.find('=');
            if (equals == std::string::npos)
            {
                continue;
            }

            const std::string key = line.substr(0, equals);
            const std::string value = line.substr(equals + 1);

            if (key == "name")
            {
                outPrefab.Name = value;
            }
            else if (key == "mesh")
            {
                outPrefab.MeshName = value;
            }
            else if (key == "scale")
            {
                outPrefab.TransformData.Scale = ParseFloat3(value, outPrefab.TransformData.Scale);
            }
            else if (key == "albedo")
            {
                outPrefab.MaterialData.Albedo = ParseFloat3(value, outPrefab.MaterialData.Albedo);
            }
            else if (key == "roughness")
            {
                outPrefab.MaterialData.Roughness = std::stof(value);
            }
            else if (key == "metallic")
            {
                outPrefab.MaterialData.Metallic = std::stof(value);
            }
            else if (key == "alpha")
            {
                outPrefab.MaterialData.Alpha = std::stof(value);
            }
        }

        return true;
    }

    bool PrefabIO::Save(const std::filesystem::path& path, const Prefab& prefab)
    {
        std::ostringstream output;
        output << "# DISPARITY prefab v1\n";
        output << "name=" << prefab.Name << '\n';
        output << "mesh=" << prefab.MeshName << '\n';
        output << "scale="
            << prefab.TransformData.Scale.x << ','
            << prefab.TransformData.Scale.y << ','
            << prefab.TransformData.Scale.z << '\n';
        output << "albedo="
            << prefab.MaterialData.Albedo.x << ','
            << prefab.MaterialData.Albedo.y << ','
            << prefab.MaterialData.Albedo.z << '\n';
        output << "roughness=" << prefab.MaterialData.Roughness << '\n';
        output << "metallic=" << prefab.MaterialData.Metallic << '\n';
        output << "alpha=" << prefab.MaterialData.Alpha << '\n';
        return FileSystem::WriteTextFile(path, output.str());
    }

    Prefab PrefabIO::FromSceneObject(const NamedSceneObject& object)
    {
        Prefab prefab;
        prefab.Name = object.Name;
        prefab.MeshName = object.MeshName;
        prefab.TransformData = object.Object.TransformData;
        prefab.TransformData.Position = {};
        prefab.MaterialData = object.Object.MaterialData;
        return prefab;
    }

    NamedSceneObject PrefabIO::Instantiate(
        const Prefab& prefab,
        const std::string& instanceName,
        const DirectX::XMFLOAT3& position,
        const std::unordered_map<std::string, MeshHandle>& meshes)
    {
        NamedSceneObject object;
        object.Name = instanceName.empty() ? prefab.Name : instanceName;
        object.MeshName = prefab.MeshName;
        object.Object.TransformData = prefab.TransformData;
        object.Object.TransformData.Position = position;
        object.Object.MaterialData = prefab.MaterialData;

        const auto found = meshes.find(prefab.MeshName);
        object.Object.Mesh = found == meshes.end() ? 0 : found->second;
        return object;
    }
}
