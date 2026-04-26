#pragma once

#include "Disparity/Scene/Material.h"

#include <filesystem>
#include <string>

namespace Disparity
{
    struct MaterialAsset
    {
        std::string Name = "Material";
        Material MaterialData;
        std::filesystem::path BaseColorTexturePath;
    };

    class MaterialAssetIO
    {
    public:
        [[nodiscard]] static bool Load(const std::filesystem::path& path, MaterialAsset& outMaterial);
        [[nodiscard]] static bool Save(const std::filesystem::path& path, const MaterialAsset& material);
    };
}
