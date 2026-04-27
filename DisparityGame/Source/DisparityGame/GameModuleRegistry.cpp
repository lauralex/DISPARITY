#include "DisparityGame/GameModuleRegistry.h"

#include <algorithm>
#include <set>

namespace DisparityGame
{
    std::vector<GameModuleDescriptor> BuildV36GameModuleRegistry()
    {
        return {
            {
                "DISPARITY layer",
                "Game",
                "DisparityGame.cpp",
                "The legacy layer still owns live gameplay/editor orchestration while extracted modules take over stable catalogs and inventories.",
                false,
                true,
                true
            },
            {
                "v36 followup catalog",
                "Production",
                "DisparityGame/GameFollowupCatalog.cpp",
                "Stable public verification points are isolated from the runtime layer to keep future batches easier to review.",
                false,
                false,
                false
            },
            {
                "game module inventory",
                "Production",
                "DisparityGame/GameModuleRegistry.cpp",
                "Readable module metadata lets verification and docs track the gradual source split.",
                false,
                true,
                true
            },
            {
                "service registry",
                "Engine",
                "Disparity/Core/ServiceRegistry.cpp",
                "Engine service readiness is now owned by the library instead of ad hoc game counters.",
                true,
                true,
                true
            },
            {
                "telemetry stream",
                "Engine",
                "Disparity/Diagnostics/TelemetryStream.cpp",
                "Structured counters, gauges, and events provide a shared reporting route for engine, editor, and game systems.",
                true,
                true,
                true
            },
            {
                "config variable registry",
                "Runtime",
                "Disparity/Runtime/ConfigVarRegistry.cpp",
                "Typed runtime tuning values prepare the engine for editor-driven settings and future console commands.",
                true,
                true,
                true
            },
            {
                "editor panel registry",
                "Editor",
                "Disparity/Editor/EditorPanelRegistry.cpp",
                "Panel order and visibility are centralized for future layout persistence work.",
                true,
                true,
                false
            }
        };
    }

    GameModuleRegistryDiagnostics SummarizeGameModules(const std::vector<GameModuleDescriptor>& modules)
    {
        GameModuleRegistryDiagnostics diagnostics;
        diagnostics.ModuleCount = static_cast<uint32_t>(modules.size());

        std::set<std::string> sourceFiles;
        for (const GameModuleDescriptor& module : modules)
        {
            diagnostics.EngineOwnedModules += module.EngineOwned ? 1u : 0u;
            diagnostics.EditorFacingModules += module.EditorFacing ? 1u : 0u;
            diagnostics.GameplayFacingModules += module.GameplayFacing ? 1u : 0u;
            diagnostics.CommentedModules += module.Notes.empty() ? 0u : 1u;
            if (!module.SourceFile.empty())
            {
                sourceFiles.insert(module.SourceFile);
            }
        }

        diagnostics.SplitSourceFiles = static_cast<uint32_t>(sourceFiles.size());
        return diagnostics;
    }
}
