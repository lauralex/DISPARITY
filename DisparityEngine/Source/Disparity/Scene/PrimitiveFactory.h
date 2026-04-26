#pragma once

#include "Disparity/Scene/Mesh.h"

namespace Disparity::PrimitiveFactory
{
    [[nodiscard]] MeshData CreateCube();
    [[nodiscard]] MeshData CreatePlane(float size);
    [[nodiscard]] MeshData CreateTorus(float majorRadius, float minorRadius, unsigned int majorSegments, unsigned int minorSegments);
    [[nodiscard]] MeshData CreateTerrainGrid(unsigned int cellsPerSide, float size, float heightScale);
}
