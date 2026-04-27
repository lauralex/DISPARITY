#include "Disparity/Assets/MaterialAsset.h"

#include "Disparity/Core/FileSystem.h"

#include <sstream>
#include <string_view>

namespace Disparity
{
    namespace
    {
        bool StartsWith(const std::string& value, const char* prefix)
        {
            const std::string_view text(value);
            const std::string_view prefixView(prefix);
            return text.size() >= prefixView.size() && text.substr(0, prefixView.size()) == prefixView;
        }

        DirectX::XMFLOAT3 ParseFloat3(const std::string& value, DirectX::XMFLOAT3 fallback)
        {
            std::stringstream stream(value);
            char comma = 0;
            DirectX::XMFLOAT3 result = fallback;
            stream >> result.x >> comma >> result.y >> comma >> result.z;
            return stream.fail() ? fallback : result;
        }
    }

    bool MaterialAssetIO::Load(const std::filesystem::path& path, MaterialAsset& outMaterial)
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
                outMaterial.Name = value;
            }
            else if (key == "albedo")
            {
                outMaterial.MaterialData.Albedo = ParseFloat3(value, outMaterial.MaterialData.Albedo);
            }
            else if (key == "roughness")
            {
                outMaterial.MaterialData.Roughness = std::stof(value);
            }
            else if (key == "metallic")
            {
                outMaterial.MaterialData.Metallic = std::stof(value);
            }
            else if (key == "alpha")
            {
                outMaterial.MaterialData.Alpha = std::stof(value);
            }
            else if (key == "emissive")
            {
                outMaterial.MaterialData.Emissive = ParseFloat3(value, outMaterial.MaterialData.Emissive);
            }
            else if (key == "emissive_intensity")
            {
                outMaterial.MaterialData.EmissiveIntensity = std::stof(value);
            }
            else if (key == "texture" && !StartsWith(value, "#"))
            {
                outMaterial.BaseColorTexturePath = value;
            }
        }

        return true;
    }

    bool MaterialAssetIO::Save(const std::filesystem::path& path, const MaterialAsset& material)
    {
        std::ostringstream output;
        output << "# DISPARITY material v1\n";
        output << "name=" << material.Name << '\n';
        output << "albedo=" << material.MaterialData.Albedo.x << ',' << material.MaterialData.Albedo.y << ',' << material.MaterialData.Albedo.z << '\n';
        output << "roughness=" << material.MaterialData.Roughness << '\n';
        output << "metallic=" << material.MaterialData.Metallic << '\n';
        output << "alpha=" << material.MaterialData.Alpha << '\n';
        output << "emissive=" << material.MaterialData.Emissive.x << ',' << material.MaterialData.Emissive.y << ',' << material.MaterialData.Emissive.z << '\n';
        output << "emissive_intensity=" << material.MaterialData.EmissiveIntensity << '\n';
        output << "texture=" << material.BaseColorTexturePath.string() << '\n';
        return FileSystem::WriteTextFile(path, output.str());
    }
}
