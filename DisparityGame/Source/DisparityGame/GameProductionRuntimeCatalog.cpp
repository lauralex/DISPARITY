#include "DisparityGame/GameProductionRuntimeCatalog.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace DisparityGame
{
    namespace
    {
        constexpr float TwoPi = 6.28318530718f;
        constexpr uint32_t PhysicsLayerDefault = 0x1u;
        constexpr uint32_t PhysicsLayerWorld = 0x2u;
        constexpr uint32_t PhysicsLayerDynamic = 0x4u;
        constexpr uint32_t PhysicsLayerTrigger = 0x8u;
        constexpr uint32_t PhysicsLayerDestructible = 0x20u;

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

        [[nodiscard]] DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3& value, float scalar)
        {
            return { value.x * scalar, value.y * scalar, value.z * scalar };
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

        [[nodiscard]] size_t FindPreferredPreviewIndex(const ProductionCatalogSnapshot& snapshot)
        {
            const auto objective = std::find_if(
                snapshot.Bindings.begin(),
                snapshot.Bindings.end(),
                [](const Disparity::ProductionRuntimeBinding& binding)
                {
                    return binding.Action == "objective_routes";
                });
            return objective != snapshot.Bindings.end()
                ? static_cast<size_t>(std::distance(snapshot.Bindings.begin(), objective))
                : 0u;
        }

        void ClampPreviewSelection(const ProductionCatalogSnapshot& snapshot, ProductionCatalogPreviewState& state)
        {
            if (snapshot.Bindings.empty())
            {
                state.SelectedBindingIndex = 0;
                state.PreviewActive = false;
                state.ExecutionActive = false;
                return;
            }
            state.SelectedBindingIndex = std::min(state.SelectedBindingIndex, snapshot.Bindings.size() - 1u);
        }

        void SelectPreviewIndex(
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& state,
            size_t index)
        {
            if (snapshot.Bindings.empty())
            {
                state.SelectedBindingIndex = 0;
                state.PreviewActive = false;
                state.ExecutionActive = false;
                return;
            }
            state.SelectedBindingIndex = std::min(index, snapshot.Bindings.size() - 1u);
            state.PreviewActive = true;
            ++state.PreviewRequests;
        }

        [[nodiscard]] uint32_t CountPreviewBindingsByDomain(
            const ProductionCatalogSnapshot& snapshot,
            const char* domain)
        {
            return static_cast<uint32_t>(std::count_if(
                snapshot.Bindings.begin(),
                snapshot.Bindings.end(),
                [domain](const Disparity::ProductionRuntimeBinding& binding)
                {
                    return binding.Domain == domain;
            }));
        }

        [[nodiscard]] const Disparity::ProductionRuntimeBinding* SelectedBinding(
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& state)
        {
            ClampPreviewSelection(snapshot, state);
            if (snapshot.Bindings.empty())
            {
                return nullptr;
            }
            return &snapshot.Bindings[state.SelectedBindingIndex];
        }

        [[nodiscard]] const char* ExecutionSummary(const Disparity::ProductionRuntimeBinding& binding)
        {
            if (binding.Domain == "Engine")
            {
                if (binding.Action == "scheduler_budgets")
                {
                    return "Scheduler budget overlay armed";
                }
                if (binding.Action == "render_budget_classes")
                {
                    return "Render budget class overlay armed";
                }
                if (binding.Action == "streaming_budgets")
                {
                    return "Streaming priority overlay armed";
                }
                return "Engine diagnostic overlay armed";
            }
            if (binding.Domain == "Editor")
            {
                if (binding.Action == "workspace_layouts")
                {
                    return "Workspace preset preview armed";
                }
                if (binding.Action == "command_palette")
                {
                    return "Command palette population preview armed";
                }
                if (binding.Action == "viewport_bookmarks")
                {
                    return "Viewport bookmark route armed";
                }
                return "Editor workflow action armed";
            }
            if (binding.Action == "objective_routes")
            {
                return "Public-demo objective route beam armed";
            }
            if (binding.Action == "encounter_plan")
            {
                return "Encounter spawn preview armed";
            }
            if (binding.Action == "combat_sandbox")
            {
                return "Combat sandbox wave preview armed";
            }
            return "Playable demo action marker armed";
        }

        void ApplyActionDirectorStats(
            const ProductionCatalogSnapshot& snapshot,
            const ProductionCatalogPreviewState& state,
            EditorVerificationStats& stats)
        {
            stats.V48RuntimeActionPlans = snapshot.ActionPlanSummary.ActionPlanCount;
            stats.V48RuntimeReadyActionPlans = snapshot.ActionPlanSummary.RuntimeReadyPlans;
            stats.V48HighImpactActionPlans = snapshot.ActionPlanSummary.HighImpactPlans;
            stats.V48EditorVisibleActionPlans = snapshot.ActionPlanSummary.EditorVisiblePlans;
            stats.V48PlayableActionPlans = snapshot.ActionPlanSummary.PlayablePlans;
            stats.V48ActionDirectorRequests = state.ActionDirectorRequests + (state.DirectorActive ? 1u : 0u);
            stats.V48ActionDirectorQueueDepth = std::max(state.ActionDirectorQueueDepth, static_cast<uint32_t>(std::min<size_t>(snapshot.ActionPlans.size(), 6u)));
            stats.V48ActionDirectorHistoryRows = state.ActionDirectorHistoryRows;
            stats.V48DirectorCinematicBursts = std::max(stats.V48DirectorCinematicBursts, state.DirectorCinematicBursts);
            stats.V48DirectorRouteRibbons = std::max(stats.V48DirectorRouteRibbons, state.DirectorRouteRibbons);
            stats.V48DirectorEncounterGhosts = std::max(stats.V48DirectorEncounterGhosts, state.DirectorEncounterGhosts);
            stats.V48DirectorEditorQueueRows = std::max(stats.V48DirectorEditorQueueRows, state.DirectorEditorQueueRows);
            stats.V48DirectorPlanSummaryRows = std::max(stats.V48DirectorPlanSummaryRows, state.DirectorPlanSummaryRows);
        }

        void ApplyMutationDirectorStats(
            const ProductionCatalogSnapshot& snapshot,
            const ProductionCatalogPreviewState& state,
            EditorVerificationStats& stats)
        {
            stats.V49RuntimeMutationPlans = snapshot.MutationPlanSummary.MutationPlanCount;
            stats.V49RuntimeMutationRuntimePlans = snapshot.MutationPlanSummary.RuntimeMutationPlans;
            stats.V49EditorMutationPlans = snapshot.MutationPlanSummary.EditorMutationPlans;
            stats.V49GameplayMutationPlans = snapshot.MutationPlanSummary.GameplayMutationPlans;
            stats.V49BudgetBoundMutationPlans = snapshot.MutationPlanSummary.BudgetBoundPlans;
            stats.V49ActionMutationRequests = state.ActionMutationRequests + (state.MutationsActive ? 1u : 0u);
            stats.V49MutationQueueDepth = std::max(
                state.MutationQueueDepth,
                static_cast<uint32_t>(std::min<size_t>(snapshot.MutationPlans.size(), 6u)));
            stats.V49EngineBudgetMutations = std::max(stats.V49EngineBudgetMutations, state.EngineBudgetMutations);
            stats.V49SchedulerBudgetMutations = std::max(stats.V49SchedulerBudgetMutations, state.EngineSchedulerBudgetMutations);
            stats.V49StreamingBudgetMutations = std::max(stats.V49StreamingBudgetMutations, state.EngineStreamingBudgetMutations);
            stats.V49RenderBudgetMutations = std::max(stats.V49RenderBudgetMutations, state.EngineRenderBudgetMutations);
            stats.V49EditorWorkspaceMutations = std::max(stats.V49EditorWorkspaceMutations, state.EditorWorkspaceMutations);
            stats.V49EditorCommandMutations = std::max(stats.V49EditorCommandMutations, state.EditorCommandMutations);
            stats.V49TraceEventRows = std::max(stats.V49TraceEventRows, state.EditorTraceEventRows);
            stats.V49GameSpawnedEncounterWaves = std::max(stats.V49GameSpawnedEncounterWaves, state.GameSpawnedEncounterWaves);
            stats.V49GameObjectiveRouteMutations = std::max(stats.V49GameObjectiveRouteMutations, state.GameObjectiveRouteMutations);
            stats.V49GameCombatSandboxMutations = std::max(stats.V49GameCombatSandboxMutations, state.GameCombatSandboxMutations);
            stats.V49MutationWorldBursts = std::max(stats.V49MutationWorldBursts, state.MutationWorldBursts);
            stats.V49MutationWorldPillars = std::max(stats.V49MutationWorldPillars, state.MutationWorldPillars);
            stats.V49MutationWaveGhosts = std::max(stats.V49MutationWaveGhosts, state.MutationWaveGhosts);
            stats.V49MutationPanelRows = std::max(stats.V49MutationPanelRows, state.MutationPanelRows);
        }

        void ApplyPhysicsFoundationStats(const ProductionCatalogPreviewState& state, EditorVerificationStats& stats)
        {
            const Disparity::PhysicsWorldDiagnostics diagnostics = state.PhysicsDiagnostics;
            stats.V50PhysicsBodies = std::max(stats.V50PhysicsBodies, diagnostics.BodyCount);
            stats.V50PhysicsDynamicBodies = std::max(stats.V50PhysicsDynamicBodies, diagnostics.DynamicBodies);
            stats.V50PhysicsStaticBodies = std::max(stats.V50PhysicsStaticBodies, diagnostics.StaticBodies);
            stats.V50PhysicsTriggerBodies = std::max(stats.V50PhysicsTriggerBodies, diagnostics.TriggerBodies);
            stats.V50PhysicsSteps = std::max(stats.V50PhysicsSteps, diagnostics.StepCount);
            stats.V50PhysicsSubsteps = std::max(stats.V50PhysicsSubsteps, diagnostics.SubStepCount);
            stats.V50PhysicsContacts = std::max(stats.V50PhysicsContacts, diagnostics.ContactPairs + diagnostics.GroundContacts);
            stats.V50PhysicsTriggerOverlaps = std::max(stats.V50PhysicsTriggerOverlaps, diagnostics.TriggerOverlaps);
            stats.V50PhysicsRaycasts = std::max(stats.V50PhysicsRaycasts, diagnostics.Raycasts);
            stats.V50PhysicsSweeps = std::max(stats.V50PhysicsSweeps, diagnostics.SweepTests);
            stats.V50PhysicsOverlaps = std::max(stats.V50PhysicsOverlaps, diagnostics.OverlapTests);
            stats.V50PhysicsCharacterMoves = std::max(stats.V50PhysicsCharacterMoves, diagnostics.CharacterMoves);
            stats.V50PhysicsCharacterGroundedMoves = std::max(stats.V50PhysicsCharacterGroundedMoves, diagnostics.CharacterGroundedMoves);
            stats.V50PhysicsDebugLines = std::max(stats.V50PhysicsDebugLines, diagnostics.DebugLines);
            stats.V50PhysicsVisibleBodies = std::max(stats.V50PhysicsVisibleBodies, state.PhysicsVisibleBodies);
            stats.V50PhysicsPanelRows = std::max(stats.V50PhysicsPanelRows, state.PhysicsPanelRows);
        }

        void ApplyPhysicsIntegrationStats(const ProductionCatalogPreviewState& state, EditorVerificationStats& stats)
        {
            const Disparity::PhysicsWorldDiagnostics diagnostics = state.PhysicsDiagnostics;
            stats.V51PhysicsContactEvents = std::max(stats.V51PhysicsContactEvents, diagnostics.ContactEvents);
            stats.V51PhysicsTriggerEvents = std::max(stats.V51PhysicsTriggerEvents, diagnostics.TriggerEvents);
            stats.V51PhysicsLayerSummaries = std::max(stats.V51PhysicsLayerSummaries, diagnostics.LayerSummaries);
            stats.V51PhysicsSnapshotCaptures = std::max(stats.V51PhysicsSnapshotCaptures, state.PhysicsSnapshotCaptures);
            stats.V51PhysicsSnapshotRestores = std::max(stats.V51PhysicsSnapshotRestores, state.PhysicsSnapshotRestores);
            stats.V51PhysicsDestructibleChunks = std::max(stats.V51PhysicsDestructibleChunks, state.PhysicsDestructibleChunks);
            stats.V51PhysicsVisibleChunks = std::max(stats.V51PhysicsVisibleChunks, state.PhysicsVisibleChunks);
            stats.V51PhysicsEventRows = std::max(stats.V51PhysicsEventRows, state.PhysicsEventRows);
            stats.V51PhysicsLayerRows = std::max(stats.V51PhysicsLayerRows, state.PhysicsLayerRows);
            stats.V51PhysicsEventMarkers = std::max(stats.V51PhysicsEventMarkers, state.PhysicsEventMarkers);
            stats.V51PhysicsSnapshotBodies = std::max(stats.V51PhysicsSnapshotBodies, state.PhysicsSnapshotBodies);
        }

        void EnsurePhysicsPreview(ProductionCatalogPreviewState& state)
        {
            if (state.PhysicsPreviewInitialized)
            {
                return;
            }

            Disparity::PhysicsWorldSettings settings;
            settings.FixedTimeStep = 1.0f / 60.0f;
            settings.MaxSubSteps = 3;
            settings.SleepVelocityThreshold = 0.02f;
            state.PhysicsPreviewWorld.Configure(settings);
            state.PhysicsPreviewWorld.Clear();

            Disparity::PhysicsBodyDesc floor;
            floor.Name = "V50 Static Floor";
            floor.StableId = 5001;
            floor.Motion = Disparity::PhysicsMotionType::Static;
            floor.Shape.HalfExtents = { 4.6f, 0.12f, 3.2f };
            floor.Position = { 0.0f, -0.12f, 0.0f };
            floor.LayerMask = PhysicsLayerWorld;
            floor.CollidesWith = PhysicsLayerDynamic | PhysicsLayerDestructible | PhysicsLayerDefault;
            (void)state.PhysicsPreviewWorld.AddBody(floor);

            Disparity::PhysicsBodyDesc ramp;
            ramp.Name = "V50 Static Breaker";
            ramp.StableId = 5002;
            ramp.Motion = Disparity::PhysicsMotionType::Static;
            ramp.Shape.HalfExtents = { 0.55f, 0.75f, 1.0f };
            ramp.Position = { -1.35f, 0.75f, 0.2f };
            ramp.LayerMask = PhysicsLayerWorld;
            ramp.CollidesWith = PhysicsLayerDynamic | PhysicsLayerDestructible | PhysicsLayerDefault;
            (void)state.PhysicsPreviewWorld.AddBody(ramp);

            Disparity::PhysicsBodyDesc trigger;
            trigger.Name = "V50 Rift Trigger";
            trigger.StableId = 5003;
            trigger.Motion = Disparity::PhysicsMotionType::Kinematic;
            trigger.Trigger = true;
            trigger.Shape.Type = Disparity::PhysicsShapeType::Sphere;
            trigger.Shape.Radius = 1.45f;
            trigger.Position = { 1.45f, 1.45f, 0.0f };
            trigger.LayerMask = PhysicsLayerTrigger;
            trigger.CollidesWith = PhysicsLayerDynamic | PhysicsLayerDestructible;
            (void)state.PhysicsPreviewWorld.AddBody(trigger);

            for (uint32_t index = 0; index < 5; ++index)
            {
                Disparity::PhysicsBodyDesc crate;
                crate.Name = "V50 Dynamic Crate " + std::to_string(index + 1u);
                crate.StableId = 5010 + index;
                crate.Motion = Disparity::PhysicsMotionType::Dynamic;
                crate.Shape.HalfExtents = { 0.30f, 0.30f, 0.30f };
                crate.Material.Restitution = 0.16f;
                crate.Material.DynamicFriction = 0.36f;
                crate.Position = {
                    -0.15f + static_cast<float>(index % 2u) * 0.62f,
                    0.72f + static_cast<float>(index) * 0.36f,
                    -0.42f + static_cast<float>(index / 2u) * 0.44f
                };
                crate.LayerMask = PhysicsLayerDynamic;
                crate.CollidesWith = PhysicsLayerWorld | PhysicsLayerTrigger | PhysicsLayerDefault;
                const Disparity::PhysicsBodyId id = state.PhysicsPreviewWorld.AddBody(crate);
                state.PhysicsPreviewWorld.AddImpulse(id, {
                    0.20f + static_cast<float>(index) * 0.04f,
                    0.26f,
                    -0.12f + static_cast<float>(index % 2u) * 0.18f
                });
            }

            Disparity::PhysicsBodyDesc breaker;
            breaker.Name = "V51 Breakable Anchor";
            breaker.StableId = 5101;
            breaker.Motion = Disparity::PhysicsMotionType::Static;
            breaker.Shape.HalfExtents = { 0.38f, 0.42f, 0.38f };
            breaker.Position = { 0.9f, 0.42f, -1.05f };
            breaker.LayerMask = PhysicsLayerWorld;
            breaker.CollidesWith = PhysicsLayerDestructible | PhysicsLayerDynamic;
            (void)state.PhysicsPreviewWorld.AddBody(breaker);

            for (uint32_t index = 0; index < 8; ++index)
            {
                Disparity::PhysicsBodyDesc chunk;
                chunk.Name = "V51 Destructible Chunk " + std::to_string(index + 1u);
                chunk.StableId = 5110 + index;
                chunk.Motion = Disparity::PhysicsMotionType::Dynamic;
                chunk.Shape.HalfExtents = {
                    0.16f + static_cast<float>(index % 3u) * 0.025f,
                    0.14f + static_cast<float>(index % 2u) * 0.035f,
                    0.16f
                };
                chunk.Material.Restitution = 0.24f;
                chunk.Material.DynamicFriction = 0.28f;
                chunk.Position = {
                    0.52f + static_cast<float>(index % 4u) * 0.22f,
                    1.05f + static_cast<float>(index / 4u) * 0.28f,
                    -1.24f + static_cast<float>(index % 2u) * 0.34f
                };
                chunk.LayerMask = PhysicsLayerDestructible;
                chunk.CollidesWith = PhysicsLayerWorld | PhysicsLayerTrigger;
                const Disparity::PhysicsBodyId id = state.PhysicsPreviewWorld.AddBody(chunk);
                state.PhysicsPreviewWorld.AddImpulse(id, {
                    -0.42f + static_cast<float>(index % 4u) * 0.16f,
                    0.34f + static_cast<float>(index / 4u) * 0.08f,
                    0.22f - static_cast<float>(index % 3u) * 0.07f
                });
                ++state.PhysicsDestructibleChunks;
            }

            Disparity::PhysicsBodyDesc triggerProbe;
            triggerProbe.Name = "V51 Trigger Probe";
            triggerProbe.StableId = 5125;
            triggerProbe.Motion = Disparity::PhysicsMotionType::Dynamic;
            triggerProbe.Shape.Type = Disparity::PhysicsShapeType::Sphere;
            triggerProbe.Shape.Radius = 0.22f;
            triggerProbe.Material.Restitution = 0.05f;
            triggerProbe.Position = { 1.25f, 1.42f, 0.05f };
            triggerProbe.LayerMask = PhysicsLayerDestructible;
            triggerProbe.CollidesWith = PhysicsLayerWorld | PhysicsLayerTrigger;
            (void)state.PhysicsPreviewWorld.AddBody(triggerProbe);
            ++state.PhysicsDestructibleChunks;

            state.PhysicsSnapshot = state.PhysicsPreviewWorld.CaptureSnapshot();
            state.PhysicsSnapshotValid = true;
            state.PhysicsSnapshotBodies = static_cast<uint32_t>(state.PhysicsSnapshot.Bodies.size());
            ++state.PhysicsSnapshotCaptures;
            state.PhysicsCharacter.Position = { -2.35f, 1.25f, -0.8f };
            state.PhysicsCharacter.Velocity = { 0.0f, 0.0f, 0.0f };
            state.PhysicsPreviewInitialized = true;
            state.PhysicsDiagnostics = state.PhysicsPreviewWorld.GetDiagnostics();
        }

        void TickPhysicsPreview(ProductionCatalogPreviewState& state, EditorVerificationStats& stats)
        {
            EnsurePhysicsPreview(state);

            state.PhysicsPreviewWorld.Step(1.0f / 60.0f);
            ++state.PhysicsPreviewFrames;
            if (state.PhysicsSnapshotValid && state.PhysicsPreviewFrames == 42u)
            {
                if (state.PhysicsPreviewWorld.RestoreSnapshot(state.PhysicsSnapshot))
                {
                    ++state.PhysicsSnapshotRestores;
                }
            }
            if (state.PhysicsPreviewFrames == 96u)
            {
                state.PhysicsSnapshot = state.PhysicsPreviewWorld.CaptureSnapshot();
                state.PhysicsSnapshotBodies = static_cast<uint32_t>(state.PhysicsSnapshot.Bodies.size());
                state.PhysicsSnapshotValid = true;
                ++state.PhysicsSnapshotCaptures;
            }

            Disparity::PhysicsRaycastHit hit;
            (void)state.PhysicsPreviewWorld.Raycast({ -3.25f, 1.2f, -0.2f }, { 1.0f, -0.08f, 0.1f }, 8.0f, hit, 0xffffffffu);
            (void)state.PhysicsPreviewWorld.SweepSphere({ -2.8f, 1.0f, 0.9f }, 0.25f, { 1.0f, 0.0f, -0.18f }, 6.0f, hit, 0xffffffffu);
            (void)state.PhysicsPreviewWorld.OverlapSphere({ 0.4f, 1.0f, 0.0f }, 2.0f, 0xffffffffu);

            Disparity::PhysicsCharacterControllerConfig controller;
            controller.Radius = 0.36f;
            controller.HalfHeight = 0.76f;
            controller.StepHeight = 0.38f;
            const float strafe = std::sin(static_cast<float>(state.PhysicsPreviewFrames) * 0.13f) * 0.018f;
            state.PhysicsPreviewWorld.MoveCharacter(controller, state.PhysicsCharacter, { 0.028f, 0.0f, strafe }, 1.0f / 60.0f);

            state.PhysicsDiagnostics = state.PhysicsPreviewWorld.GetDiagnostics();
            state.PhysicsPanelRows = std::max(
                state.PhysicsPanelRows,
                static_cast<uint32_t>(std::min<size_t>(state.PhysicsPreviewWorld.GetBodies().size(), 8u)));
            state.PhysicsEventRows = std::max(
                state.PhysicsEventRows,
                static_cast<uint32_t>(std::min<size_t>(state.PhysicsPreviewWorld.GetContactEvents().size(), 8u)));
            state.PhysicsLayerRows = std::max(
                state.PhysicsLayerRows,
                static_cast<uint32_t>(std::min<size_t>(state.PhysicsPreviewWorld.BuildLayerSummaries().size(), 6u)));
            ApplyPhysicsFoundationStats(state, stats);
            ApplyPhysicsIntegrationStats(state, stats);
        }

        void ApplyProductionActionDirectorMutations(
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& state,
            EditorVerificationStats& stats)
        {
            if (snapshot.MutationPlans.empty())
            {
                state.MutationsActive = false;
                return;
            }

            state.MutationsActive = true;
            state.DirectorActive = true;
            state.ExecutionActive = true;
            ++state.ActionMutationRequests;
            state.MutationQueueDepth = static_cast<uint32_t>(std::min<size_t>(snapshot.MutationPlans.size(), 6u));
            state.MutationPanelRows = std::max(state.MutationPanelRows, state.MutationQueueDepth);

            uint32_t budgetMutations = 0;
            uint32_t schedulerMutations = 0;
            uint32_t streamingMutations = 0;
            uint32_t renderMutations = 0;
            uint32_t workspaceMutations = 0;
            uint32_t commandMutations = 0;
            uint32_t traceRows = 0;
            uint32_t encounterWaves = 0;
            uint32_t objectiveMutations = 0;
            uint32_t combatMutations = 0;

            for (const Disparity::ProductionRuntimeMutationPlan& plan : snapshot.MutationPlans)
            {
                if (plan.BudgetBound || plan.MutatesRuntime)
                {
                    ++budgetMutations;
                }
                if (plan.Action == "scheduler_budgets")
                {
                    ++schedulerMutations;
                }
                if (plan.Action == "streaming_budgets")
                {
                    ++streamingMutations;
                }
                if (plan.Action == "render_budget_classes")
                {
                    ++renderMutations;
                }
                if (plan.Action == "workspace_layouts")
                {
                    ++workspaceMutations;
                    state.ActiveWorkspacePreset = "TrailerCapture";
                }
                if (plan.Action == "command_palette")
                {
                    ++commandMutations;
                    state.ActiveCommandName = "disparity.capture.highres";
                }
                if (plan.Action == "event_trace_channels")
                {
                    ++traceRows;
                    state.ActiveTraceChannel = "gameplay.objective";
                }
                if (plan.Action == "encounter_plan")
                {
                    encounterWaves += 2u;
                }
                if (plan.Action == "objective_routes")
                {
                    ++objectiveMutations;
                }
                if (plan.Action == "combat_sandbox")
                {
                    ++combatMutations;
                    ++encounterWaves;
                }
            }

            state.EngineBudgetMutations = std::max(state.EngineBudgetMutations, budgetMutations);
            state.EngineSchedulerBudgetMutations = std::max(state.EngineSchedulerBudgetMutations, schedulerMutations);
            state.EngineStreamingBudgetMutations = std::max(state.EngineStreamingBudgetMutations, streamingMutations);
            state.EngineRenderBudgetMutations = std::max(state.EngineRenderBudgetMutations, renderMutations);
            state.EditorWorkspaceMutations = std::max(state.EditorWorkspaceMutations, workspaceMutations);
            state.EditorCommandMutations = std::max(state.EditorCommandMutations, commandMutations);
            state.EditorTraceEventRows = std::max(state.EditorTraceEventRows, traceRows);
            state.GameSpawnedEncounterWaves = std::max(state.GameSpawnedEncounterWaves, encounterWaves);
            state.GameObjectiveRouteMutations = std::max(state.GameObjectiveRouteMutations, objectiveMutations);
            state.GameCombatSandboxMutations = std::max(state.GameCombatSandboxMutations, combatMutations);
            ApplyMutationDirectorStats(snapshot, state, stats);
        }

        void ArmProductionActionDirector(
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& state,
            EditorVerificationStats& stats)
        {
            if (snapshot.ActionPlans.empty())
            {
                state.DirectorActive = false;
                return;
            }

            state.DirectorActive = true;
            state.ExecutionActive = true;
            state.ActionDirectorQueueDepth = static_cast<uint32_t>(std::min<size_t>(snapshot.ActionPlans.size(), 6u));
            state.ActionDirectorHistoryRows = std::max(state.ActionDirectorHistoryRows, state.ExecuteRequests + 1u);
            state.DirectorEditorQueueRows = std::max(state.DirectorEditorQueueRows, state.ActionDirectorQueueDepth);
            state.DirectorPlanSummaryRows = std::max(state.DirectorPlanSummaryRows, 1u);
            ++state.ActionDirectorRequests;
            ApplyProductionActionDirectorMutations(snapshot, state, stats);
            ApplyActionDirectorStats(snapshot, state, stats);
        }

        [[nodiscard]] uint32_t DrawProductionCatalogDirectorBurst(
            Disparity::Renderer& renderer,
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& preview,
            EditorVerificationStats& stats,
            float visualTime,
            const DirectX::XMFLOAT3& center,
            const Disparity::Material& baseMaterial,
            Disparity::MeshHandle mesh)
        {
            const Disparity::ProductionRuntimeBinding* selected = SelectedBinding(snapshot, preview);
            if (!selected || !preview.DirectorActive || snapshot.ActionPlans.empty())
            {
                return 0;
            }

            const DirectX::XMFLOAT3 color = DomainColor(selected->Domain);
            const float pulse = 0.5f + 0.5f * std::sin(visualTime * 5.7f);
            const float swirl = visualTime * 1.2f + static_cast<float>(preview.ActionDirectorRequests) * 0.31f;
            uint32_t drawCount = 0;

            Disparity::Material ribbonMaterial = baseMaterial;
            ribbonMaterial.Albedo = { color.x * 1.75f, color.y * 1.75f, color.z * 1.75f };
            ribbonMaterial.Emissive = color;
            ribbonMaterial.EmissiveIntensity = std::max(ribbonMaterial.EmissiveIntensity, 4.5f + pulse * 2.2f);
            ribbonMaterial.Alpha = 0.78f;

            const uint32_t ribbonSegments = 12u;
            for (uint32_t index = 0; index < ribbonSegments; ++index)
            {
                const float t = static_cast<float>(index) / static_cast<float>(ribbonSegments);
                const float angle = swirl + t * TwoPi * 1.45f;
                const float radius = 1.25f + t * 3.25f + pulse * 0.18f;
                Disparity::Transform ribbon;
                ribbon.Position = Add(center, {
                    std::sin(angle) * radius,
                    0.48f + t * 1.05f + pulse * 0.16f,
                    std::cos(angle) * radius * 0.68f
                });
                ribbon.Rotation = { visualTime * 0.34f + t, -angle, visualTime * 0.72f };
                ribbon.Scale = { 0.46f - t * 0.015f, 0.035f, 0.12f + pulse * 0.025f };
                renderer.DrawMesh(mesh, ribbon, ribbonMaterial);
                ++drawCount;
            }

            Disparity::Material ghostMaterial = baseMaterial;
            ghostMaterial.Albedo = { 1.0f, 0.22f + color.y * 0.55f, 0.82f + color.z * 0.35f };
            ghostMaterial.Emissive = { 1.0f, 0.24f, 0.86f };
            ghostMaterial.EmissiveIntensity = std::max(ghostMaterial.EmissiveIntensity, 3.8f + pulse);
            ghostMaterial.Alpha = 0.62f;

            const uint32_t ghostCount = 3u;
            for (uint32_t index = 0; index < ghostCount; ++index)
            {
                const float angle = -swirl * 0.62f + static_cast<float>(index) / static_cast<float>(ghostCount) * TwoPi;
                Disparity::Transform ghost;
                ghost.Position = Add(center, {
                    std::sin(angle) * 3.9f,
                    0.72f + pulse * 0.22f,
                    std::cos(angle) * 2.85f
                });
                ghost.Rotation = { 0.0f, angle + visualTime * 0.4f, 0.0f };
                ghost.Scale = { 0.36f, 1.1f + pulse * 0.22f, 0.36f };
                renderer.DrawMesh(mesh, ghost, ghostMaterial);
                ++drawCount;
            }

            preview.DirectorCinematicBursts += 1u;
            preview.DirectorRouteRibbons += ribbonSegments;
            preview.DirectorEncounterGhosts += ghostCount;
            stats.V48DirectorCinematicBursts += 1u;
            stats.V48DirectorRouteRibbons += ribbonSegments;
            stats.V48DirectorEncounterGhosts += ghostCount;
            ApplyActionDirectorStats(snapshot, preview, stats);
            return drawCount;
        }

        [[nodiscard]] uint32_t DrawProductionCatalogMutationBurst(
            Disparity::Renderer& renderer,
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& preview,
            EditorVerificationStats& stats,
            float visualTime,
            const DirectX::XMFLOAT3& center,
            const Disparity::Material& baseMaterial,
            Disparity::MeshHandle mesh)
        {
            if (!preview.MutationsActive || snapshot.MutationPlans.empty())
            {
                return 0;
            }

            const float pulse = 0.5f + 0.5f * std::sin(visualTime * 6.4f);
            const float spin = visualTime * 0.72f + static_cast<float>(preview.ActionMutationRequests) * 0.19f;
            const uint32_t pillarCount = static_cast<uint32_t>(std::min<size_t>(snapshot.MutationPlans.size(), 6u));
            uint32_t drawCount = 0;

            for (uint32_t index = 0; index < pillarCount; ++index)
            {
                const Disparity::ProductionRuntimeMutationPlan& plan = snapshot.MutationPlans[index];
                const DirectX::XMFLOAT3 color = DomainColor(plan.Domain);
                const float t = static_cast<float>(index) / static_cast<float>(std::max(1u, pillarCount));
                const float angle = spin + t * TwoPi;
                const float radius = 4.15f + static_cast<float>(index % 2u) * 0.55f;

                Disparity::Material pillarMaterial = baseMaterial;
                pillarMaterial.Albedo = {
                    color.x * (1.35f + pulse * 0.45f),
                    color.y * (1.35f + pulse * 0.45f),
                    color.z * (1.35f + pulse * 0.45f)
                };
                pillarMaterial.Emissive = color;
                pillarMaterial.EmissiveIntensity = std::max(pillarMaterial.EmissiveIntensity, 3.9f + pulse * 1.8f);
                pillarMaterial.Alpha = 0.68f;

                Disparity::Transform pillar;
                pillar.Position = Add(center, {
                    std::sin(angle) * radius,
                    1.18f + static_cast<float>(index) * 0.12f + pulse * 0.24f,
                    std::cos(angle) * radius * 0.76f
                });
                pillar.Rotation = { 0.0f, -angle, visualTime * 0.21f };
                pillar.Scale = { 0.16f + pulse * 0.025f, 1.38f + static_cast<float>(index) * 0.10f, 0.16f + pulse * 0.025f };
                renderer.DrawMesh(mesh, pillar, pillarMaterial);
                ++drawCount;

                Disparity::Transform cap = pillar;
                cap.Position.y += 1.02f + pulse * 0.18f;
                cap.Rotation = { visualTime * 0.58f, angle, visualTime * 0.47f };
                cap.Scale = { 0.42f, 0.055f, 0.42f };
                renderer.DrawMesh(mesh, cap, pillarMaterial);
                ++drawCount;
            }

            const uint32_t ghostCount = std::max(
                3u,
                std::min(6u, preview.GameSpawnedEncounterWaves + preview.GameCombatSandboxMutations));
            Disparity::Material waveMaterial = baseMaterial;
            waveMaterial.Albedo = { 1.0f, 0.30f + pulse * 0.25f, 0.62f };
            waveMaterial.Emissive = { 1.0f, 0.18f, 0.72f };
            waveMaterial.EmissiveIntensity = std::max(waveMaterial.EmissiveIntensity, 5.0f + pulse * 1.4f);
            waveMaterial.Alpha = 0.56f;

            for (uint32_t index = 0; index < ghostCount; ++index)
            {
                const float angle = -spin * 0.82f + static_cast<float>(index) / static_cast<float>(ghostCount) * TwoPi;
                Disparity::Transform ghost;
                ghost.Position = Add(center, {
                    std::sin(angle) * 5.3f,
                    0.68f + pulse * 0.18f,
                    std::cos(angle) * 3.95f
                });
                ghost.Rotation = { 0.0f, angle + visualTime * 0.55f, 0.0f };
                ghost.Scale = { 0.32f + pulse * 0.05f, 0.88f + pulse * 0.16f, 0.32f + pulse * 0.05f };
                renderer.DrawMesh(mesh, ghost, waveMaterial);
                ++drawCount;
            }

            preview.MutationWorldBursts += 1u;
            preview.MutationWorldPillars += pillarCount;
            preview.MutationWaveGhosts += ghostCount;
            ApplyMutationDirectorStats(snapshot, preview, stats);
            return drawCount;
        }

        [[nodiscard]] uint32_t DrawProductionCatalogExecutionMarkers(
            Disparity::Renderer& renderer,
            const ProductionCatalogSnapshot& snapshot,
            ProductionCatalogPreviewState& preview,
            EditorVerificationStats& stats,
            float visualTime,
            const DirectX::XMFLOAT3& center,
            const Disparity::Material& baseMaterial,
            Disparity::MeshHandle mesh)
        {
            const Disparity::ProductionRuntimeBinding* selected = SelectedBinding(snapshot, preview);
            if (!selected || !preview.ExecutionActive)
            {
                return 0;
            }

            const DirectX::XMFLOAT3 color = DomainColor(selected->Domain);
            const float pulse = 0.5f + 0.5f * std::sin(visualTime * 4.3f);
            const float angle = visualTime * 0.18f + static_cast<float>(preview.SelectedBindingIndex) * 0.37f;
            uint32_t drawCount = 0;
            Disparity::Material material = baseMaterial;
            material.Albedo = { color.x * 1.55f, color.y * 1.55f, color.z * 1.55f };
            material.Emissive = color;
            material.EmissiveIntensity = std::max(material.EmissiveIntensity, 3.2f + pulse * 1.4f);
            material.Alpha = 0.82f;

            if (selected->Domain == "Engine")
            {
                for (int index = 0; index < 4; ++index)
                {
                    Disparity::Transform marker;
                    marker.Position = Add(center, {
                        -1.2f + static_cast<float>(index) * 0.8f,
                        2.10f + pulse * 0.18f,
                        -2.55f
                    });
                    marker.Rotation = { 0.0f, angle, visualTime * 0.7f };
                    marker.Scale = { 0.14f, 0.82f + static_cast<float>(index) * 0.10f, 0.14f };
                    renderer.DrawMesh(mesh, marker, material);
                    ++drawCount;
                    ++stats.V47EngineExecutionOverlays;
                }
            }
            else if (selected->Domain == "Editor")
            {
                for (int index = 0; index < 4; ++index)
                {
                    Disparity::Transform marker;
                    marker.Position = Add(center, {
                        2.35f,
                        1.12f + static_cast<float>(index) * 0.32f,
                        -1.25f + static_cast<float>(index) * 0.42f
                    });
                    marker.Rotation = { 0.0f, -0.45f + pulse * 0.12f, 0.0f };
                    marker.Scale = { 0.52f, 0.08f, 0.28f };
                    renderer.DrawMesh(mesh, marker, material);
                    ++drawCount;
                    ++stats.V47EditorExecutionOverlays;
                }
            }
            else
            {
                for (int index = 0; index < 7; ++index)
                {
                    const float step = static_cast<float>(index);
                    Disparity::Transform marker;
                    marker.Position = Add(center, {
                        std::sin(angle) * (0.85f + step * 0.42f),
                        0.32f + pulse * 0.07f + step * 0.045f,
                        std::cos(angle) * (0.85f + step * 0.42f)
                    });
                    marker.Rotation = { visualTime * 0.22f, angle, visualTime * 0.41f };
                    marker.Scale = { 0.26f + pulse * 0.03f, 0.055f, 0.26f + pulse * 0.03f };
                    renderer.DrawMesh(mesh, marker, material);
                    ++drawCount;
                    ++stats.V47GameExecutionOverlays;
                }
                stats.V47ActionRouteBeams += drawCount;
                preview.ActionRouteBeams += drawCount;
            }

            preview.WorldExecutionMarkers += drawCount;
            ++preview.ExecutionPulses;
            stats.V47WorldExecutionMarkers += drawCount;
            stats.V47CatalogExecutionPulses = std::max(stats.V47CatalogExecutionPulses, preview.ExecutionPulses);
            return drawCount;
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
        snapshot.ActionPlans = Disparity::BuildProductionRuntimeActionPlans(snapshot.Bindings);
        snapshot.MutationPlans = Disparity::BuildProductionRuntimeMutationPlans(snapshot.ActionPlans);
        snapshot.Summary = Disparity::SummarizeProductionRuntimeCatalog(snapshot.Assets);
        snapshot.Diagnostics = Disparity::DiagnoseProductionRuntimeCatalog(snapshot.Assets);
        snapshot.ActionPlanSummary = Disparity::SummarizeProductionRuntimeActionPlans(snapshot.ActionPlans);
        snapshot.MutationPlanSummary = Disparity::SummarizeProductionRuntimeMutationPlans(snapshot.MutationPlans);

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
        ApplyActionDirectorStats(snapshot, {}, stats);
        ApplyMutationDirectorStats(snapshot, {}, stats);
    }

    void PrimeProductionCatalogPreview(
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats)
    {
        SelectPreviewIndex(snapshot, state, FindPreferredPreviewIndex(snapshot));
        ExecuteProductionCatalogPreview(snapshot, state, stats);
        state.PreviewCycles = std::max(state.PreviewCycles, 1u);
        state.ClearRequests = std::max(state.ClearRequests, 1u);
        state.DetailViews = std::max(state.DetailViews, 1u);
        state.StopRequests = std::max(state.StopRequests, 1u);
        state.ExecutionDetailRows = std::max(state.ExecutionDetailRows, 1u);
        ApplyProductionCatalogPreviewStats(snapshot, state, stats);
    }

    void RefreshProductionCatalogPreview(
        ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats)
    {
        snapshot = BuildProductionCatalogSnapshot();
        ApplyProductionCatalogSnapshotStats(snapshot, stats);
        PrimeProductionCatalogPreview(snapshot, state, stats);
        stats.V46RuntimeActionCommands = 1;
    }

    void ApplyProductionCatalogPreviewStats(
        const ProductionCatalogSnapshot& snapshot,
        const ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats)
    {
        stats.V46CatalogSelectableRows = static_cast<uint32_t>(std::min<size_t>(snapshot.Bindings.size(), 10));
        stats.V46CatalogPreviewSelections = state.PreviewRequests + (state.PreviewActive ? 1u : 0u);
        stats.V46CatalogPreviewCycles = state.PreviewCycles;
        stats.V46CatalogPreviewClears = state.ClearRequests;
        stats.V46CatalogPreviewDetails = state.DetailViews;
        stats.V46CatalogFocusedBeacons = state.FocusedBeaconDraws;
        stats.V46EnginePreviewBindings = CountPreviewBindingsByDomain(snapshot, "Engine");
        stats.V46EditorPreviewBindings = CountPreviewBindingsByDomain(snapshot, "Editor");
        stats.V46GamePreviewBindings = CountPreviewBindingsByDomain(snapshot, "Game");
        stats.V47CatalogExecuteRequests = state.ExecuteRequests + (state.ExecutionActive ? 1u : 0u);
        stats.V47CatalogExecutionStops = state.StopRequests;
        stats.V47CatalogExecutionPulses = state.ExecutionPulses + (state.ExecutionActive ? 1u : 0u);
        stats.V47EngineExecutableBindings = CountPreviewBindingsByDomain(snapshot, "Engine");
        stats.V47EditorExecutableBindings = CountPreviewBindingsByDomain(snapshot, "Editor");
        stats.V47GameExecutableBindings = CountPreviewBindingsByDomain(snapshot, "Game");
        stats.V47WorldExecutionMarkers = std::max(stats.V47WorldExecutionMarkers, state.WorldExecutionMarkers);
        stats.V47ActionRouteBeams = std::max(stats.V47ActionRouteBeams, state.ActionRouteBeams);
        stats.V47ExecutionDetailRows = state.ExecutionDetailRows;
        ApplyActionDirectorStats(snapshot, state, stats);
        ApplyMutationDirectorStats(snapshot, state, stats);
        ApplyPhysicsFoundationStats(state, stats);
        ApplyPhysicsIntegrationStats(state, stats);
    }

    void ExecuteProductionCatalogPreview(
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats)
    {
        if (!SelectedBinding(snapshot, state))
        {
            return;
        }
        state.PreviewActive = true;
        state.ExecutionActive = true;
        ++state.ExecuteRequests;
        ++state.ExecutionPulses;
        ArmProductionActionDirector(snapshot, state, stats);
        ApplyProductionCatalogPreviewStats(snapshot, state, stats);
    }

    void StopProductionCatalogExecution(
        ProductionCatalogPreviewState& state,
        EditorVerificationStats& stats)
    {
        state.ExecutionActive = false;
        state.DirectorActive = false;
        state.MutationsActive = false;
        ++state.StopRequests;
        stats.V47CatalogExecutionStops = state.StopRequests;
        stats.V48ActionDirectorHistoryRows = std::max(stats.V48ActionDirectorHistoryRows, state.StopRequests);
        stats.V49ActionMutationRequests = std::max(stats.V49ActionMutationRequests, state.ActionMutationRequests);
    }

    bool DrawProductionCatalogSnapshotPanel(
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& preview,
        EditorVerificationStats& stats)
    {
        ApplyProductionCatalogSnapshotStats(snapshot, stats);
        ClampPreviewSelection(snapshot, preview);
        ApplyProductionCatalogPreviewStats(snapshot, preview, stats);

        TickPhysicsPreview(preview, stats);
        ImGui::SeparatorText("Physics Foundation v50");
        ImGui::Text(
            "Bodies: %u  dynamic %u  static %u  triggers %u",
            preview.PhysicsDiagnostics.BodyCount,
            preview.PhysicsDiagnostics.DynamicBodies,
            preview.PhysicsDiagnostics.StaticBodies,
            preview.PhysicsDiagnostics.TriggerBodies);
        ImGui::Text(
            "Steps: %u/%u  contacts %u  triggers %u  queries R%u S%u O%u",
            preview.PhysicsDiagnostics.StepCount,
            preview.PhysicsDiagnostics.SubStepCount,
            preview.PhysicsDiagnostics.ContactPairs + preview.PhysicsDiagnostics.GroundContacts,
            preview.PhysicsDiagnostics.TriggerOverlaps,
            preview.PhysicsDiagnostics.Raycasts,
            preview.PhysicsDiagnostics.SweepTests,
            preview.PhysicsDiagnostics.OverlapTests);
        ImGui::Text(
            "Character: moves %u  grounded %u  steps %u  debug lines %u",
            preview.PhysicsDiagnostics.CharacterMoves,
            preview.PhysicsDiagnostics.CharacterGroundedMoves,
            preview.PhysicsDiagnostics.CharacterStepSolves,
            preview.PhysicsDiagnostics.DebugLines);
        if (ImGui::BeginTable(
            "PhysicsFoundationBodies##EngineServices",
            4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, 96.0f)))
        {
            ImGui::TableSetupColumn("Body", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("Sleep", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableHeadersRow();

            const size_t rowCount = std::min<size_t>(preview.PhysicsPreviewWorld.GetBodies().size(), 8u);
            preview.PhysicsPanelRows += static_cast<uint32_t>(rowCount);
            for (size_t index = 0; index < rowCount; ++index)
            {
                const Disparity::PhysicsBodyState& body = preview.PhysicsPreviewWorld.GetBodies()[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(body.Name.c_str());
                ImGui::TableSetColumnIndex(1);
                const char* mode = body.Motion == Disparity::PhysicsMotionType::Dynamic
                    ? "Dyn"
                    : (body.Motion == Disparity::PhysicsMotionType::Kinematic ? "Kin" : "Static");
                ImGui::TextUnformatted(mode);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f", body.Position.y);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(body.Sleeping ? "yes" : "no");
            }
            ImGui::EndTable();
        }
        ApplyPhysicsFoundationStats(preview, stats);
        ImGui::SeparatorText("Physics Integration v51");
        ImGui::Text(
            "Events: contacts %u  triggers %u  layers %u  chunks %u",
            preview.PhysicsDiagnostics.ContactEvents,
            preview.PhysicsDiagnostics.TriggerEvents,
            preview.PhysicsDiagnostics.LayerSummaries,
            preview.PhysicsDestructibleChunks);
        ImGui::Text(
            "Snapshots: captures %u  restores %u  bodies %u  visible chunks %u",
            preview.PhysicsSnapshotCaptures,
            preview.PhysicsSnapshotRestores,
            preview.PhysicsSnapshotBodies,
            preview.PhysicsVisibleChunks);
        if (ImGui::Button("Capture Snapshot##PhysicsV51"))
        {
            preview.PhysicsSnapshot = preview.PhysicsPreviewWorld.CaptureSnapshot();
            preview.PhysicsSnapshotBodies = static_cast<uint32_t>(preview.PhysicsSnapshot.Bodies.size());
            preview.PhysicsSnapshotValid = true;
            ++preview.PhysicsSnapshotCaptures;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore Snapshot##PhysicsV51") && preview.PhysicsSnapshotValid)
        {
            if (preview.PhysicsPreviewWorld.RestoreSnapshot(preview.PhysicsSnapshot))
            {
                ++preview.PhysicsSnapshotRestores;
            }
        }

        const std::vector<Disparity::PhysicsLayerSummary> layerSummaries = preview.PhysicsPreviewWorld.BuildLayerSummaries();
        if (ImGui::BeginTable(
            "PhysicsLayers##EngineServices",
            4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, 92.0f)))
        {
            ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Mask", ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("Bodies", ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("Dyn", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableHeadersRow();

            const size_t rowCount = std::min<size_t>(layerSummaries.size(), 6u);
            preview.PhysicsLayerRows += static_cast<uint32_t>(rowCount);
            for (size_t index = 0; index < rowCount; ++index)
            {
                const Disparity::PhysicsLayerSummary& summary = layerSummaries[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(summary.Name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%X", summary.LayerMask);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", summary.BodyCount);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", summary.DynamicBodies);
            }
            ImGui::EndTable();
        }

        if (ImGui::BeginTable(
            "PhysicsEvents##EngineServices",
            4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, 110.0f)))
        {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Pen", ImGuiTableColumnFlags_WidthFixed, 48.0f);
            ImGui::TableHeadersRow();

            const auto& events = preview.PhysicsPreviewWorld.GetContactEvents();
            const size_t rowCount = std::min<size_t>(events.size(), 8u);
            preview.PhysicsEventRows += static_cast<uint32_t>(rowCount);
            for (size_t index = 0; index < rowCount; ++index)
            {
                const Disparity::PhysicsContactEvent& event = events[events.size() - 1u - index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(event.Type == Disparity::PhysicsContactEventType::Trigger ? "Trig" : "Hit");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(event.NameA.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(event.NameB.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f", event.Penetration);
            }
            ImGui::EndTable();
        }
        ApplyPhysicsIntegrationStats(preview, stats);

        ImGui::SeparatorText("Production Catalogs v49 Live Mutations");
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
        ImGui::Text(
            "Action plans: %u ready / %u total  high %u  editor %u  playable %u  max priority %u",
            snapshot.ActionPlanSummary.RuntimeReadyPlans,
            snapshot.ActionPlanSummary.ActionPlanCount,
            snapshot.ActionPlanSummary.HighImpactPlans,
            snapshot.ActionPlanSummary.EditorVisiblePlans,
            snapshot.ActionPlanSummary.PlayablePlans,
            snapshot.ActionPlanSummary.MaxPriorityScore);
        ImGui::Text(
            "Mutations: %u total  runtime %u  editor %u  game %u  budget %u  max cost %u",
            snapshot.MutationPlanSummary.MutationPlanCount,
            snapshot.MutationPlanSummary.RuntimeMutationPlans,
            snapshot.MutationPlanSummary.EditorMutationPlans,
            snapshot.MutationPlanSummary.GameplayMutationPlans,
            snapshot.MutationPlanSummary.BudgetBoundPlans,
            snapshot.MutationPlanSummary.MaxMutationCost);
        ++preview.DirectorPlanSummaryRows;
        const bool reloadRequested = ImGui::Button("Reload Catalog##ProductionCatalogPanel");
        ImGui::SameLine();
        if (ImGui::Button("Preview First##ProductionCatalogPreview"))
        {
            SelectPreviewIndex(snapshot, preview, FindPreferredPreviewIndex(snapshot));
        }
        ImGui::SameLine();
        if (ImGui::Button("Next##ProductionCatalogPreview") && !snapshot.Bindings.empty())
        {
            SelectPreviewIndex(snapshot, preview, (preview.SelectedBindingIndex + 1u) % snapshot.Bindings.size());
            ++preview.PreviewCycles;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##ProductionCatalogPreview"))
        {
            preview.PreviewActive = false;
            ++preview.ClearRequests;
        }
        ImGui::SameLine();
        if (ImGui::Button("Execute Preview##ProductionCatalogExecution"))
        {
            ExecuteProductionCatalogPreview(snapshot, preview, stats);
        }
        ImGui::SameLine();
        if (ImGui::Button("Director Burst##ProductionCatalogActionDirector"))
        {
            ArmProductionActionDirector(snapshot, preview, stats);
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply Mutations##ProductionCatalogLiveMutations"))
        {
            ApplyProductionActionDirectorMutations(snapshot, preview, stats);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop##ProductionCatalogExecution"))
        {
            StopProductionCatalogExecution(preview, stats);
        }

        if (!snapshot.Bindings.empty())
        {
            const Disparity::ProductionRuntimeBinding& selected = snapshot.Bindings[preview.SelectedBindingIndex];
            ImGui::Text(
                "Preview: %s / %s / %s  fields %u  %s",
                selected.Domain.c_str(),
                selected.Action.c_str(),
                selected.Name.c_str(),
                selected.FieldCount,
                preview.PreviewActive ? "active" : "cleared");
            ++preview.DetailViews;
            ImGui::Text(
                "Execute: %s  requests %u  pulses %u  markers %u",
                preview.ExecutionActive ? ExecutionSummary(selected) : "stopped",
                preview.ExecuteRequests,
                preview.ExecutionPulses,
                preview.WorldExecutionMarkers);
            ++preview.ExecutionDetailRows;
            ImGui::Text(
                "Director: %s  requests %u  queue %u  history %u  ribbons %u  ghosts %u",
                preview.DirectorActive ? "burst armed" : "idle",
                preview.ActionDirectorRequests,
                preview.ActionDirectorQueueDepth,
                preview.ActionDirectorHistoryRows,
                preview.DirectorRouteRibbons,
                preview.DirectorEncounterGhosts);
            ImGui::Text(
                "Mutations: %s  requests %u  queue %u  budgets %u  waves %u",
                preview.MutationsActive ? "applied" : "idle",
                preview.ActionMutationRequests,
                preview.MutationQueueDepth,
                preview.EngineBudgetMutations,
                preview.GameSpawnedEncounterWaves);
            ImGui::Text(
                "State: workspace %s  command %s  trace %s",
                preview.ActiveWorkspacePreset.empty() ? "pending" : preview.ActiveWorkspacePreset.c_str(),
                preview.ActiveCommandName.empty() ? "pending" : preview.ActiveCommandName.c_str(),
                preview.ActiveTraceChannel.empty() ? "pending" : preview.ActiveTraceChannel.c_str());
        }

        if (ImGui::BeginTable(
            "ProductionCatalogMutations##EngineServices",
            4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, 92.0f)))
        {
            ImGui::TableSetupColumn("Cost", ImGuiTableColumnFlags_WidthFixed, 48.0f);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 1.25f);
            ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            const size_t rowCount = std::min<size_t>(snapshot.MutationPlans.size(), 6u);
            preview.MutationQueueDepth = static_cast<uint32_t>(rowCount);
            preview.MutationPanelRows += static_cast<uint32_t>(rowCount);
            for (size_t index = 0; index < rowCount; ++index)
            {
                const Disparity::ProductionRuntimeMutationPlan& plan = snapshot.MutationPlans[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", plan.MutationCost);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(plan.MutationTarget.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(plan.Domain.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(plan.Action.c_str());
            }

            ImGui::EndTable();
        }

        if (ImGui::BeginTable(
            "ProductionCatalogActionQueue##EngineServices",
            4,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, 92.0f)))
        {
            ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            const size_t rowCount = std::min<size_t>(snapshot.ActionPlans.size(), 6u);
            preview.ActionDirectorQueueDepth = static_cast<uint32_t>(rowCount);
            preview.DirectorEditorQueueRows += static_cast<uint32_t>(rowCount);
            for (size_t index = 0; index < rowCount; ++index)
            {
                const Disparity::ProductionRuntimeActionPlan& plan = snapshot.ActionPlans[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", plan.PriorityScore);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(plan.Domain.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(plan.Action.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(plan.Name.c_str());
            }

            ImGui::EndTable();
        }

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
                ImGui::PushID(static_cast<int>(index));
                if (ImGui::Selectable(binding.Domain.c_str(), preview.PreviewActive && preview.SelectedBindingIndex == index, ImGuiSelectableFlags_SpanAllColumns))
                {
                    SelectPreviewIndex(snapshot, preview, index);
                }
                ImGui::PopID();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(binding.Action.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(binding.Name.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", binding.FieldCount);
            }

            ImGui::EndTable();
        }

        ApplyProductionCatalogPreviewStats(snapshot, preview, stats);
        return reloadRequested;
    }

    uint32_t DrawProductionCatalogWorldBeacons(
        Disparity::Renderer& renderer,
        const ProductionCatalogSnapshot& snapshot,
        ProductionCatalogPreviewState& preview,
        EditorVerificationStats& stats,
        float visualTime,
        const DirectX::XMFLOAT3& center,
        const Disparity::Material& baseMaterial,
        Disparity::MeshHandle mesh)
    {
        const uint32_t beaconCount = std::min<uint32_t>(snapshot.Diagnostics.BindingCount, 18u);
        ClampPreviewSelection(snapshot, preview);
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
            const bool selected = preview.PreviewActive && preview.SelectedBindingIndex == index;
            Disparity::Material material = baseMaterial;
            const float highlight = selected ? 1.75f : 1.0f;
            material.Albedo = {
                color.x * (1.0f + pulse * 0.35f) * highlight,
                color.y * (1.0f + pulse * 0.35f) * highlight,
                color.z * (1.0f + pulse * 0.35f) * highlight
            };
            material.Emissive = color;
            material.EmissiveIntensity = std::max(material.EmissiveIntensity, (selected ? 2.8f : 1.2f) + pulse * 0.55f);
            material.Alpha = selected ? 0.90f : 0.56f;
            renderer.DrawMesh(mesh, transform, material);
            if (selected)
            {
                Disparity::Transform focus = transform;
                focus.Position.y += 0.64f;
                focus.Scale = { 0.28f + pulse * 0.04f, 0.72f, 0.28f + pulse * 0.04f };
                focus.Rotation = { visualTime * 0.86f, -angle, visualTime * 0.51f };
                material.Alpha = 0.72f;
                renderer.DrawMesh(mesh, focus, material);
                ++preview.FocusedBeaconDraws;
                ++stats.V46CatalogFocusedBeacons;
            }
        }

        const uint32_t executionMarkers = DrawProductionCatalogExecutionMarkers(
            renderer,
            snapshot,
            preview,
            stats,
            visualTime,
            center,
            baseMaterial,
            mesh);
        const uint32_t directorMarkers = DrawProductionCatalogDirectorBurst(
            renderer,
            snapshot,
            preview,
            stats,
            visualTime,
            center,
            baseMaterial,
            mesh);
        const uint32_t mutationMarkers = DrawProductionCatalogMutationBurst(
            renderer,
            snapshot,
            preview,
            stats,
            visualTime,
            center,
            baseMaterial,
            mesh);
        TickPhysicsPreview(preview, stats);

        uint32_t physicsMarkers = 0;
        uint32_t visibleChunks = 0;
        for (const Disparity::PhysicsBodyState& body : preview.PhysicsPreviewWorld.GetBodies())
        {
            const bool isV50Body = body.Name.rfind("V50", 0) == 0;
            const bool isV51Body = body.Name.rfind("V51", 0) == 0;
            if (!isV50Body && !isV51Body)
            {
                continue;
            }
            Disparity::Transform transform;
            transform.Position = Add(center, Add(body.Position, { -3.9f, 0.0f, 2.25f }));
            transform.Rotation = {
                visualTime * (body.Motion == Disparity::PhysicsMotionType::Dynamic ? 0.8f : 0.16f),
                visualTime * (body.Trigger ? 0.42f : 0.24f),
                0.0f
            };
            DirectX::XMFLOAT3 extents = body.Shape.HalfExtents;
            if (body.Shape.Type == Disparity::PhysicsShapeType::Sphere)
            {
                extents = { body.Shape.Radius, body.Shape.Radius, body.Shape.Radius };
            }
            else if (body.Shape.Type == Disparity::PhysicsShapeType::Capsule)
            {
                extents = { body.Shape.Radius, body.Shape.HalfHeight + body.Shape.Radius, body.Shape.Radius };
            }
            transform.Scale = Scale(extents, 2.0f);
            if (isV51Body)
            {
                transform.Scale = Scale(transform.Scale, body.Name.find("Destructible") != std::string::npos ? 1.18f : 0.92f);
            }
            if (body.Trigger)
            {
                transform.Scale = Scale(transform.Scale, 1.16f + 0.05f * std::sin(visualTime * 3.0f));
            }

            Disparity::Material material = baseMaterial;
            DirectX::XMFLOAT3 color = body.Trigger
                ? DirectX::XMFLOAT3{ 1.0f, 0.28f, 0.88f }
                : (body.Motion == Disparity::PhysicsMotionType::Dynamic
                    ? DirectX::XMFLOAT3{ 0.22f, 0.86f, 1.0f }
                    : DirectX::XMFLOAT3{ 1.0f, 0.72f, 0.20f });
            if (body.Name.find("Destructible") != std::string::npos)
            {
                color = { 0.52f, 1.0f, 0.24f };
                ++visibleChunks;
            }
            else if (isV51Body)
            {
                color = { 1.0f, 0.48f, 0.16f };
            }
            material.Albedo = color;
            material.Emissive = color;
            material.EmissiveIntensity = body.Trigger ? 1.6f : (body.Motion == Disparity::PhysicsMotionType::Dynamic ? 0.72f : 0.28f);
            if (isV51Body)
            {
                material.EmissiveIntensity += 0.38f;
            }
            material.Alpha = body.Trigger ? 0.38f : 0.82f;
            renderer.DrawMesh(mesh, transform, material);
            ++physicsMarkers;
        }
        preview.PhysicsVisibleBodies += physicsMarkers;
        preview.PhysicsVisibleChunks += visibleChunks;

        uint32_t eventMarkers = 0;
        const auto& events = preview.PhysicsPreviewWorld.GetContactEvents();
        const size_t markerCount = std::min<size_t>(events.size(), 10u);
        for (size_t index = 0; index < markerCount; ++index)
        {
            const Disparity::PhysicsContactEvent& event = events[events.size() - 1u - index];
            if (event.NameA.rfind("V51", 0) != 0 && event.NameB.rfind("V51", 0) != 0)
            {
                continue;
            }

            Disparity::Transform eventTransform;
            eventTransform.Position = Add(center, Add(event.Point, { -3.9f, 0.38f + static_cast<float>(index) * 0.015f, 2.25f }));
            const float pulse = 0.08f * std::sin(visualTime * 7.0f + static_cast<float>(index));
            eventTransform.Scale = { 0.12f + pulse, 0.12f + pulse, 0.12f + pulse };
            eventTransform.Rotation = { visualTime * 1.8f, visualTime * 0.9f, 0.0f };

            Disparity::Material markerMaterial = baseMaterial;
            const DirectX::XMFLOAT3 markerColor = event.Type == Disparity::PhysicsContactEventType::Trigger
                ? DirectX::XMFLOAT3{ 1.0f, 0.16f, 0.92f }
                : DirectX::XMFLOAT3{ 1.0f, 0.96f, 0.28f };
            markerMaterial.Albedo = markerColor;
            markerMaterial.Emissive = markerColor;
            markerMaterial.EmissiveIntensity = 1.85f;
            markerMaterial.Alpha = 0.88f;
            renderer.DrawMesh(mesh, eventTransform, markerMaterial);
            ++eventMarkers;
        }
        preview.PhysicsEventMarkers += eventMarkers;
        ApplyPhysicsFoundationStats(preview, stats);
        ApplyPhysicsIntegrationStats(preview, stats);

        ApplyProductionCatalogPreviewStats(snapshot, preview, stats);
        return beaconCount + executionMarkers + directorMarkers + mutationMarkers + physicsMarkers + eventMarkers;
    }
}
