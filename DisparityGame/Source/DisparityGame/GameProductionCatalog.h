#pragma once

#include "DisparityGame/GameRuntimeTypes.h"

namespace DisparityGame
{
    const std::array<ProductionFollowupPoint, V25ProductionPointCount>& GetV25ProductionPoints();
    const std::array<ProductionFollowupPoint, V28DiversifiedPointCount>& GetV28DiversifiedPoints();
    const std::array<ProductionFollowupPoint, V29PublicDemoPointCount>& GetV29PublicDemoPoints();
    const std::array<ProductionFollowupPoint, V30VerticalSlicePointCount>& GetV30VerticalSlicePoints();
    const std::array<ProductionFollowupPoint, V31DiversifiedPointCount>& GetV31DiversifiedPoints();
    const std::array<ProductionFollowupPoint, V32RoadmapPointCount>& GetV32RoadmapPoints();
    const std::array<ProductionFollowupPoint, V33PlayableDemoPointCount>& GetV33PlayableDemoPoints();
    const std::array<ProductionFollowupPoint, V34AAAFoundationPointCount>& GetV34AAAFoundationPoints();
    const std::array<ProductionFollowupPoint, V35EngineArchitecturePointCount>& GetV35EngineArchitecturePoints();
}
