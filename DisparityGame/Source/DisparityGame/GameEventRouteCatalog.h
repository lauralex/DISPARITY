#pragma once

#include <cstdint>
#include <vector>

namespace DisparityGame
{
    struct GameEventRouteDescriptor
    {
        const char* Name = "";
        const char* Domain = "";
        const char* TelemetryCounter = "";
        const char* EventChannel = "";
        const char* Description = "";
        bool TelemetryBacked = false;
        bool EventBusBacked = false;
        bool Replayable = false;
        bool LinksSelection = false;
        const char* BreadcrumbLabel = "";
        const char* TraceChannel = "";
    };

    struct GameEventRouteDiagnostics
    {
        uint32_t RouteCount = 0;
        uint32_t ObjectiveRoutes = 0;
        uint32_t EncounterRoutes = 0;
        uint32_t TraversalRoutes = 0;
        uint32_t AudioRoutes = 0;
        uint32_t FailureRoutes = 0;
        uint32_t TelemetryBackedRoutes = 0;
        uint32_t EventBusBackedRoutes = 0;
        uint32_t DocumentedRoutes = 0;
        uint32_t ReplayableRoutes = 0;
        uint32_t SelectionLinkedRoutes = 0;
        uint32_t BreadcrumbRoutes = 0;
        uint32_t TraceChannels = 0;
    };

    [[nodiscard]] std::vector<GameEventRouteDescriptor> BuildPublicDemoEventRouteCatalog();
    [[nodiscard]] GameEventRouteDiagnostics SummarizeGameEventRoutes(const std::vector<GameEventRouteDescriptor>& routes);
}
