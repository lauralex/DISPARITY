#pragma once

#include "Disparity/Scene/Mesh.h"

namespace Disparity::PrimitiveFactory
{
    [[nodiscard]] MeshData CreateCube();
    [[nodiscard]] MeshData CreatePlane(float size);
    [[nodiscard]] MeshData CreateTerrainGrid(unsigned int cellsPerSide, float size, float heightScale);
}
