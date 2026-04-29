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
                "production followup catalog",
                "Production",
                "DisparityGame/GameProductionCatalog.cpp",
                "Legacy v25-v35 verification catalogs are separated from the live layer so gameplay/editor code is easier to review.",
                false,
                false,
                false
            },
            {
                "runtime helper functions",
                "Game",
                "DisparityGame/GameRuntimeHelpers.cpp",
                "Shared math, material-loading, and command-line parsing helpers are centralized outside the main layer.",
                false,
                false,
                true
            },
            {
                "runtime type surface",
                "Game",
                "DisparityGame/GameRuntimeTypes.h",
                "Prototype data shapes and verification structs are documented in one header instead of living inline in DisparityGame.cpp.",
                false,
                true,
                true
            },
            {
                "game event route catalog",
                "Game",
                "DisparityGame/GameEventRouteCatalog.cpp",
                "Public-demo objective, encounter, traversal, audio, and failure routes are cataloged outside the live layer for v38 verification.",
                false,
                true,
                true
            },
            {
                "roadmap batch evaluation",
                "Verification",
                "DisparityGame/GameRoadmapBatch.cpp",
                "The v39 roadmap point evaluation and runtime-report writing live outside the orchestration layer so verification growth stays reviewable.",
                false,
                true,
                true
            },
            {
                "production runtime catalog bridge",
                "Game",
                "DisparityGame/GameProductionRuntimeCatalog.cpp",
                "v45-v47 binds engine production catalog data into editor diagnostics and visible public-demo preview/execution markers.",
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
                "Panel order, visibility, workspace presets, search, and navigation are centralized for editor layout work.",
                true,
                true,
                false
            },
            {
                "runtime command registry",
                "Runtime",
                "Disparity/Runtime/RuntimeCommandRegistry.cpp",
                "Runtime command metadata gives engine, editor, and game systems one searchable command surface.",
                true,
                true,
                true
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
