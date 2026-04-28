#pragma once

#include <array>
#include <cstddef>

namespace DisparityGame
{
    struct GameFollowupPoint
    {
        const char* Key = "";
        const char* Domain = "";
        const char* Description = "";
    };

    constexpr size_t V36MixedBatchPointCount = 60;
    constexpr size_t V38DiversifiedBatchPointCount = 30;
    constexpr size_t V39RoadmapBatchPointCount = 15;
    constexpr size_t V40DiversifiedBatchPointCount = 15;
    constexpr size_t V41BreadthBatchPointCount = 20;
    constexpr size_t V42ProductionSurfaceBatchPointCount = 24;

    [[nodiscard]] const std::array<GameFollowupPoint, V36MixedBatchPointCount>& GetV36MixedBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V38DiversifiedBatchPointCount>& GetV38DiversifiedBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V39RoadmapBatchPointCount>& GetV39RoadmapBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V40DiversifiedBatchPointCount>& GetV40DiversifiedBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V41BreadthBatchPointCount>& GetV41BreadthBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V42ProductionSurfaceBatchPointCount>& GetV42ProductionSurfacePoints();
}
