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

    [[nodiscard]] const std::array<GameFollowupPoint, V36MixedBatchPointCount>& GetV36MixedBatchPoints();
}
