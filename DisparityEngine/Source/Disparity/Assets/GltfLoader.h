#pragma once

#include "Disparity/Scene/Mesh.h"
#include "Disparity/Scene/Material.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Disparity
{
    struct GltfMaterialInfo
    {
        std::string Name;
        Material MaterialData;
        std::filesystem::path BaseColorTexturePath;
        std::filesystem::path NormalTexturePath;
        std::filesystem::path MetallicRoughnessTexturePath;
        std::filesystem::path EmissiveTexturePath;
        std::filesystem::path OcclusionTexturePath;
    };

    struct GltfNodeInfo
    {
        std::string Name;
        int Mesh = -1;
        int Skin = -1;
        DirectX::XMFLOAT3 Translation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 Rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
        std::vector<int> Children;
    };

    struct GltfMeshPrimitiveInfo
    {
        std::string Name;
        int Material = -1;
        bool HasSkinWeights = false;
    };

    struct GltfSkinInfo
    {
        std::string Name;
        std::vector<int> Joints;
        int InverseBindMatrices = -1;
        std::vector<DirectX::XMFLOAT4X4> InverseBindMatricesData;
    };

    struct GltfAnimationSamplerInfo
    {
        std::string Interpolation = "LINEAR";
        std::vector<float> InputTimes;
        std::vector<DirectX::XMFLOAT3> OutputTranslations;
        std::vector<DirectX::XMFLOAT4> OutputRotations;
        std::vector<DirectX::XMFLOAT3> OutputScales;
    };

    struct GltfAnimationChannelInfo
    {
        int Sampler = -1;
        int TargetNode = -1;
        std::string TargetPath;
    };

    struct GltfAnimationClipInfo
    {
        std::string Name;
        std::vector<GltfAnimationSamplerInfo> Samplers;
        std::vector<GltfAnimationChannelInfo> Channels;
    };

    struct GltfSceneAsset
    {
        std::vector<MeshData> Meshes;
        std::vector<GltfMeshPrimitiveInfo> MeshPrimitives;
        std::vector<GltfMaterialInfo> Materials;
        std::vector<GltfNodeInfo> Nodes;
        std::vector<GltfSkinInfo> Skins;
        std::vector<GltfAnimationClipInfo> Animations;
    };

    class GltfLoader
    {
    public:
        [[nodiscard]] static bool LoadFirstMesh(const std::filesystem::path& path, MeshData& outMesh);
        [[nodiscard]] static bool LoadScene(const std::filesystem::path& path, GltfSceneAsset& outScene);
    };
}
