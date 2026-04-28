#include "DisparityGame/GameProductionRuntimeCatalog.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace DisparityGame
{
    namespace
    {
        constexpr float TwoPi = 6.28318530718f;

        [[nodiscard]] Disparity::ProductionRuntimeAssetRule MakeRule(
            const char* path,
            const char* requiredPrefix,
            const char* requiredToken,
            const char* activationToken,
            const char* domain,
            const char* action)
        {
            return {
                { path, requiredPrefix, requiredToken, activationToken },
                domain,
                action
            };
        }

        [[nodiscard]] DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
        {
            return { left.x + right.x, left.y + right.y, left.z + right.z };
        }

        [[nodiscard]] DirectX::XMFLOAT3 DomainColor(const std::string& domain)
        {
            if (domain == "Engine")
            {
                return { 1.0f, 0.72f, 0.20f };
            }
            if (domain == "Editor")
            {
                return { 0.22f, 0.86f, 1.0f };
            }
            return { 1.0f, 0.28f, 0.88f };
        }

    }

    std::vector<Disparity::ProductionRuntimeAssetRule> BuildProductionRuntimeCatalogRules()
    {
        return {
            MakeRule("Assets/Runtime/EventTraceChannels.deventschema", "channel", "gameplay.objective", "capture=true", "Engine", "event_trace_channels"),
            MakeRule("Assets/Runtime/SchedulerBudgets.dscheduler", "phase", "Rendering", "enforce=true", "Engine", "scheduler_budgets"),
            MakeRule("Assets/SceneSchemas/SceneQueryLayers.dqueryschema", "layer", "EnemyPerception", "broadphase=true", "Engine", "scene_query_layers"),
            MakeRule("Assets/Streaming/StreamingBudgets.dstreaming", "priority", "Critical", "async=true", "Engine", "streaming_budgets"),
            MakeRule("Assets/Rendering/RenderBudgetClasses.drenderbudget", "class", "TrailerCapture", "enforce=true", "Engine", "render_budget_classes"),
            MakeRule("Assets/Runtime/FrameTaskGraph.dtaskgraph", "edge", "Simulation->Physics", "active=true", "Engine", "frame_task_graph"),
            MakeRule("Assets/Editor/WorkspaceLayouts.dworkspace", "workspace", "TrailerCapture", "editable=true", "Editor", "workspace_layouts"),
            MakeRule("Assets/Editor/CommandPalette.dcommands", "command", "disparity.capture.highres", "search=true", "Editor", "command_palette"),
            MakeRule("Assets/Editor/ViewportBookmarks.dbookmarks", "bookmark", "RiftHero", "editable=true", "Editor", "viewport_bookmarks"),
            MakeRule("Assets/Editor/InspectorPresets.dinspector", "preset", "BeaconMaterial", "apply=true", "Editor", "inspector_presets"),
            MakeRule("Assets/Editor/DockMigrationPlan.ddockplan", "migration", "v42", "migrate=true", "Editor", "dock_migration_plan"),
            MakeRule("Assets/Cinematics/ShotTrackValidation.dshotcheck", "track", "camera_spline", "validate=true", "Editor", "shot_track_validation"),
            MakeRule("Assets/Gameplay/PublicDemoEncounterPlan.dencounter", "encounter", "ResonanceAmbush", "spawn=true", "Game", "encounter_plan"),
            MakeRule("Assets/Gameplay/PublicDemoControllerFeel.dcontroller", "preset", "PublicDemoTuned", "load=true", "Game", "controller_feel"),
            MakeRule("Assets/Gameplay/PublicDemoObjectiveRoutes.droute", "route", "extraction_complete", "trigger=true", "Game", "objective_routes"),
            MakeRule("Assets/Gameplay/PublicDemoAccessibility.daccess", "option", "high_contrast_rift", "toggle=true", "Game", "accessibility"),
            MakeRule("Assets/Gameplay/PublicDemoSaveSlots.dsaveplan", "slot", "public_demo_checkpoint", "checkpoint=true", "Game", "save_slots"),
            MakeRule("Assets/Gameplay/PublicDemoCombatSandbox.dcombat", "sandbox", "RelayPressure", "wave=true", "Game", "combat_sandbox")
        };
    }

    ProductionCatalogSnapshot BuildProductionCatalogSnapshot()
    {
        ProductionCatalogSnapshot snapshot = {};
        snapshot.Assets = Disparity::LoadProductionRuntimeCatalog(BuildProductionRuntimeCatalogRules());
        snapshot.Bindings = Disparity::BuildProductionRuntimeBindings(snapshot.Assets);
        snapshot.Summary = Disparity::SummarizeProductionRuntimeCatalog(snapshot.Assets);
        snapshot.Diagnostics = Disparity::DiagnoseProductionRuntimeCatalog(snapshot.Assets);

        const Disparity::ProductionAssetValidationRule malformedRule = {
            "Assets/Verification/V45MalformedCatalog.dprod",
            "route",
            "live=true",
            "trigger=true"
        };
        const Disparity::ProductionAssetValidationResult malformed = Disparity::ValidateProductionAsset(malformedRule);
        snapshot.NegativeFixtureRejected = malformed.Loaded && !malformed.Valid && malformed.MissingFields > 0 ? 1u : 0u;
        return snapshot;
    }

    uint32_t CountProductionCatalogBindingsByAction(
        const ProductionCatalogSnapshot& snapshot,
        const std::string& action)
    {
        return static_cast<uint32_t>(std::count_if(
            snapshot.Bindings.begin(),
            snapshot.Bindings.end(),
            [&action](const Disparity::ProductionRuntimeBinding& binding)
            {
                return binding.Action == action;
            }));
    }

    void ApplyProductionCatalogSnapshotStats(
        const ProductionCatalogSnapshot& snapshot,
        EditorVerificationStats& stats)
    {
        stats.V45RuntimeCatalogBindings = snapshot.Diagnostics.BindingCount;
        stats.V45RuntimeCatalogReadyBindings = snapshot.Diagnostics.RuntimeReadyBindings;
        stats.V45EngineCatalogBindings = snapshot.Diagnostics.EngineBindings;
        stats.V45EditorCatalogBindings = snapshot.Diagnostics.EditorBindings;
        stats.V45GameCatalogBindings = snapshot.Diagnostics.GameBindings;
        stats.V45CatalogPanelRows = static_cast<uint32_t>(std::min<size_t>(snapshot.Bindings.size(), 10u));
        stats.V45CatalogObjectiveBindings = CountProductionCatalogBindingsByAction(snapshot, "objective_routes");
        stats.V45CatalogEncounterBindings = CountProductionCatalogBindingsByAction(snapshot, "encounter_plan");
        stats.V45CatalogNegativeFixtureTests = snapshot.NegativeFixtureRejected;
    }

    bool DrawProductionCatalogSnapshotPanel(
        const ProductionCatalogSnapshot& snapshot,
        EditorVerificationStats& stats)
    {
        ApplyProductionCatalogSnapshotStats(snapshot, stats);

        ImGui::SeparatorText("Production Catalogs v45");
        ImGui::Text(
            "Bindings: %u live / %u ready  Engine %u  Editor %u  Game %u",
            snapshot.Diagnostics.BindingCount,
            snapshot.Diagnostics.RuntimeReadyBindings,
            snapshot.Diagnostics.EngineBindings,
            snapshot.Diagnostics.EditorBindings,
            snapshot.Diagnostics.GameBindings);
        ImGui::Text(
            "Assets: %u ready / %u total  fields %u  negative fixture %s",
            snapshot.Summary.RuntimeReadyAssets,
            snapshot.Summary.AssetCount,
            snapshot.Summary.FieldCount,
            snapshot.NegativeFixtureRejected != 0u ? "rejected" : "missing");
        const bool reloadRequested = ImGui::Button("Reload Catalog##ProductionCatalogPanel");

        if (ImGui::BeginTable(
            "ProductionCatalogBindings##EngineServices",
            4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, 128.0f)))
        {
            ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Fields", ImGuiTableColumnFlags_WidthFixed, 48.0f);
            ImGui::TableHeadersRow();

            const size_t rowCount = std::min<size_t>(snapshot.Bindings.size(), 10u);
            for (size_t index = 0; index < rowCount; ++index)
            {
                const Disparity::ProductionRuntimeBinding& binding = snapshot.Bindings[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(binding.Domain.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(binding.Action.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(binding.Name.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", binding.FieldCount);
            }

            ImGui::EndTable();
        }

        return reloadRequested;
    }

    uint32_t DrawProductionCatalogWorldBeacons(
        Disparity::Renderer& renderer,
        const ProductionCatalogSnapshot& snapshot,
        float visualTime,
        const DirectX::XMFLOAT3& center,
        const Disparity::Material& baseMaterial,
        Disparity::MeshHandle mesh)
    {
        const uint32_t beaconCount = std::min<uint32_t>(snapshot.Diagnostics.BindingCount, 18u);
        for (uint32_t index = 0; index < beaconCount; ++index)
        {
            const Disparity::ProductionRuntimeBinding& binding = snapshot.Bindings[index];
            const float tier = static_cast<float>(index / 6u);
            const float slice = static_cast<float>(index % 6u);
            const float angle = visualTime * (0.34f + tier * 0.06f) + slice / 6.0f * TwoPi + tier * 0.42f;
            const float radius = 3.10f + tier * 0.48f;
            const float pulse = 0.5f + 0.5f * std::sin(visualTime * 2.2f + slice);

            Disparity::Transform transform;
            transform.Position = Add(center, {
                std::sin(angle) * radius,
                1.00f + tier * 0.26f + pulse * 0.16f,
                std::cos(angle) * radius * 0.72f
            });
            transform.Rotation = { visualTime * 0.42f + tier, angle, visualTime * 0.27f + slice };
            transform.Scale = { 0.11f + pulse * 0.018f, 0.34f, 0.11f + pulse * 0.018f };

            const DirectX::XMFLOAT3 color = DomainColor(binding.Domain);
            Disparity::Material material = baseMaterial;
            material.Albedo = { color.x * (1.0f + pulse * 0.35f), color.y * (1.0f + pulse * 0.35f), color.z * (1.0f + pulse * 0.35f) };
            material.Emissive = color;
            material.EmissiveIntensity = std::max(material.EmissiveIntensity, 1.2f + pulse * 0.55f);
            material.Alpha = 0.56f;
            renderer.DrawMesh(mesh, transform, material);
        }

        return beaconCount;
    }
}
