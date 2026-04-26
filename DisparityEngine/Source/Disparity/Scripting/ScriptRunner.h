#pragma once

#include "Disparity/Scene/Scene.h"

#include <filesystem>
#include <unordered_map>

namespace Disparity
{
    class ScriptRunner
    {
    public:
        [[nodiscard]] static bool RunSceneScript(
            const std::filesystem::path& path,
            Scene& scene,
            const std::unordered_map<std::string, MeshHandle>& meshes);
    };
}
