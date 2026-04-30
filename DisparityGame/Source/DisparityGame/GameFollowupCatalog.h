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
    constexpr size_t V43LiveValidationBatchPointCount = 24;
    constexpr size_t V44RuntimeCatalogBatchPointCount = 24;
    constexpr size_t V45LiveCatalogBatchPointCount = 24;
    constexpr size_t V46CatalogActionPreviewBatchPointCount = 24;
    constexpr size_t V47CatalogExecutionBatchPointCount = 24;
    constexpr size_t V48ActionDirectorBatchPointCount = 24;
    constexpr size_t V49ActionMutationBatchPointCount = 24;

    [[nodiscard]] const std::array<GameFollowupPoint, V36MixedBatchPointCount>& GetV36MixedBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V38DiversifiedBatchPointCount>& GetV38DiversifiedBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V39RoadmapBatchPointCount>& GetV39RoadmapBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V40DiversifiedBatchPointCount>& GetV40DiversifiedBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V41BreadthBatchPointCount>& GetV41BreadthBatchPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V42ProductionSurfaceBatchPointCount>& GetV42ProductionSurfacePoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V43LiveValidationBatchPointCount>& GetV43LiveValidationPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V44RuntimeCatalogBatchPointCount>& GetV44RuntimeCatalogPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V45LiveCatalogBatchPointCount>& GetV45LiveCatalogPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V46CatalogActionPreviewBatchPointCount>& GetV46CatalogActionPreviewPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V47CatalogExecutionBatchPointCount>& GetV47CatalogExecutionPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V48ActionDirectorBatchPointCount>& GetV48ActionDirectorPoints();
    [[nodiscard]] const std::array<GameFollowupPoint, V49ActionMutationBatchPointCount>& GetV49ActionMutationPoints();
}
