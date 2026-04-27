#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DisparityGame
{
    struct GameModuleDescriptor
    {
        std::string Name;
        std::string Domain;
        std::string SourceFile;
        std::string Notes;
        bool EngineOwned = false;
        bool EditorFacing = false;
        bool GameplayFacing = false;
    };

    struct GameModuleRegistryDiagnostics
    {
        uint32_t ModuleCount = 0;
        uint32_t EngineOwnedModules = 0;
        uint32_t EditorFacingModules = 0;
        uint32_t GameplayFacingModules = 0;
        uint32_t CommentedModules = 0;
        uint32_t SplitSourceFiles = 0;
    };

    [[nodiscard]] std::vector<GameModuleDescriptor> BuildV36GameModuleRegistry();
    [[nodiscard]] GameModuleRegistryDiagnostics SummarizeGameModules(const std::vector<GameModuleDescriptor>& modules);
}
