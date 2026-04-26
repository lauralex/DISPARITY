#pragma once

#include "Disparity/Math/Transform.h"
#include "Disparity/Scene/Material.h"
#include "Disparity/Scene/Mesh.h"

namespace Disparity
{
    struct SceneObject
    {
        Transform TransformData;
        Material MaterialData;
        MeshHandle Mesh = 0;
    };
}
