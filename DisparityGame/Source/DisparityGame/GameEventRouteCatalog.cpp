#include "DisparityGame/GameEventRouteCatalog.h"

#include <set>
#include <string_view>

namespace DisparityGame
{
    namespace
    {
        GameEventRouteDescriptor MakeRoute(
            const char* name,
            const char* domain,
            const char* telemetryCounter,
            const char* eventChannel,
            const char* description,
            bool telemetryBacked,
            bool eventBusBacked,
            bool replayable,
            bool linksSelection,
            const char* breadcrumbLabel,
            const char* traceChannel,
            bool checkpointLinked,
            bool saveRelevant,
            bool chapterReplayRoute,
            bool accessibilityCritical,
            bool hudVisible)
        {
            return {
                name,
                domain,
                telemetryCounter,
                eventChannel,
                description,
                telemetryBacked,
                eventBusBacked,
                replayable,
                linksSelection,
                breadcrumbLabel,
                traceChannel,
                checkpointLinked,
                saveRelevant,
                chapterReplayRoute,
                accessibilityCritical,
                hudVisible
            };
        }
    }

    std::vector<GameEventRouteDescriptor> BuildPublicDemoEventRouteCatalog()
    {
        return {
            MakeRoute("shard_collected", "Objective", "demo.objectives", "objective", "Shard pickups advance the public-demo objective route.", true, true, true, true, "Shard secured", "objective.route", false, true, true, false, true),
            MakeRoute("phase_anchor_aligned", "Objective", "demo.objectives", "objective", "Phase anchors bridge the shard route toward the rift.", true, true, true, true, "Anchor aligned", "objective.route", true, true, true, false, true),
            MakeRoute("resonance_gate_tuned", "Objective", "demo.objectives", "objective", "Resonance gates tune after anchor completion.", true, true, true, true, "Gate tuned", "objective.route", true, true, true, false, true),
            MakeRoute("phase_relay_stabilized", "Objective", "demo.objectives", "objective", "Phase relays provide the late-route stabilization beat.", true, true, true, true, "Relay stabilized", "objective.route", true, true, true, false, true),
            MakeRoute("extraction_completed", "Objective", "demo.objectives", "objective", "Extraction completion closes the public-demo loop.", true, true, true, true, "Extraction complete", "objective.route", true, true, true, false, true),
            MakeRoute("checkpoint_saved", "Objective", "demo.objectives", "objective", "Checkpoint snapshots are recorded as replay breadcrumbs.", true, true, true, false, "Checkpoint saved", "objective.checkpoint", true, true, true, false, true),
            MakeRoute("enemy_pressure", "Encounter", "demo.enemy_pressure", "encounter", "Enemy pressure, contact, and chase state feed encounter telemetry.", true, true, true, true, "Pressure spike", "encounter.pressure", false, false, true, true, true),
            MakeRoute("enemy_reaction", "Encounter", "demo.enemy_pressure", "encounter", "Line-of-sight, telegraph, evade, and hit reactions are grouped for review.", true, true, true, true, "Enemy reaction", "encounter.pressure", false, false, true, true, true),
            MakeRoute("dash_traversal", "Traversal", "demo.traversal_actions", "traversal", "Dash and vault traversal actions are routed as gameplay events.", true, true, true, true, "Dash route", "traversal.route", false, false, true, true, true),
            MakeRoute("combo_chain", "Traversal", "demo.traversal_actions", "traversal", "Combo-route steps record traversal continuity.", true, true, true, true, "Combo step", "traversal.route", false, false, true, false, true),
            MakeRoute("content_audio_cue", "Audio", "demo.audio_cues", "audio", "Content-backed cue playback is reported through audio route metadata.", true, true, true, false, "Cue fired", "audio.route", false, false, true, true, false),
            MakeRoute("footstep_cadence", "Audio", "demo.audio_cues", "audio", "Footstep cadence pulses connect movement and audio telemetry.", true, true, true, false, "Footstep pulse", "audio.route", false, false, true, true, false),
            MakeRoute("failure_retry", "Failure", "demo.failure", "failure", "Checkpoint retries and failure presentation are traceable gameplay events.", true, true, true, true, "Retry from checkpoint", "failure.route", true, true, true, true, true),
            MakeRoute("failure_reason", "Failure", "demo.failure", "failure", "Failure reasons are captured for reviewable retry traces.", true, true, true, false, "Failure reason", "failure.route", false, true, true, true, true),
            MakeRoute("chapter_replay", "Failure", "demo.failure", "failure", "Chapter replay requests feed the same trace lane as checkpoint recovery.", true, true, true, false, "Replay chapter", "failure.route", true, true, true, true, true)
        };
    }

    GameEventRouteDiagnostics SummarizeGameEventRoutes(const std::vector<GameEventRouteDescriptor>& routes)
    {
        GameEventRouteDiagnostics diagnostics;
        diagnostics.RouteCount = static_cast<uint32_t>(routes.size());
        std::set<std::string_view> traceChannels;

        for (const GameEventRouteDescriptor& route : routes)
        {
            const std::string_view domain(route.Domain);
            diagnostics.ObjectiveRoutes += domain == "Objective" ? 1u : 0u;
            diagnostics.EncounterRoutes += domain == "Encounter" ? 1u : 0u;
            diagnostics.TraversalRoutes += domain == "Traversal" ? 1u : 0u;
            diagnostics.AudioRoutes += domain == "Audio" ? 1u : 0u;
            diagnostics.FailureRoutes += domain == "Failure" ? 1u : 0u;
            diagnostics.TelemetryBackedRoutes += route.TelemetryBacked ? 1u : 0u;
            diagnostics.EventBusBackedRoutes += route.EventBusBacked ? 1u : 0u;
            diagnostics.DocumentedRoutes += (route.Description != nullptr && route.Description[0] != '\0') ? 1u : 0u;
            diagnostics.ReplayableRoutes += route.Replayable ? 1u : 0u;
            diagnostics.SelectionLinkedRoutes += route.LinksSelection ? 1u : 0u;
            diagnostics.BreadcrumbRoutes += (route.BreadcrumbLabel != nullptr && route.BreadcrumbLabel[0] != '\0') ? 1u : 0u;
            diagnostics.CheckpointLinkedRoutes += route.CheckpointLinked ? 1u : 0u;
            diagnostics.SaveRelevantRoutes += route.SaveRelevant ? 1u : 0u;
            diagnostics.ChapterReplayRoutes += route.ChapterReplayRoute ? 1u : 0u;
            diagnostics.AccessibilityCriticalRoutes += route.AccessibilityCritical ? 1u : 0u;
            diagnostics.HudVisibleRoutes += route.HudVisible ? 1u : 0u;
            if (route.TraceChannel != nullptr && route.TraceChannel[0] != '\0')
            {
                traceChannels.insert(route.TraceChannel);
            }
        }

        diagnostics.TraceChannels = static_cast<uint32_t>(traceChannels.size());
        return diagnostics;
    }
}
