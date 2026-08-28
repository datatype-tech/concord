// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERSCENESNAPSHOT_H
#define CONCORD_RENDERSCENESNAPSHOT_H

#include "engine/core/Mat4.h"
#include "engine/core/Transform.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/scene/EnvironmentSettings.h"
#include "engine/scene/Material.h"

#include <vector>

namespace Concord {

class Scene;

/** Camera data copied from a Scene for one render frame. */
struct RenderCameraSnapshot {
    Entity entity{};
    Vec3 position{};
    Vec3 target{};
    Mat4 view{};
    Mat4 projection{};
};

/** One visible mesh and its immutable material data for a render frame. */
struct RenderObjectSnapshot {
    Entity entity{};
    Mat4 model{};
    PrimitiveShape shape = PrimitiveShape::Box;
    Vec3 size{1.0f, 1.0f, 1.0f};
    Material material{};
    bool castShadow = true;
};

/** One light and its placement copied from the scene store. */
struct RenderLightSnapshot {
    Entity entity{};
    Transform transform{};
    LightComponent light{};
};

/** Read-only render inputs extracted from one Scene at a frame boundary. */
struct RenderSceneSnapshot {
    bool hasCamera = false;
    RenderCameraSnapshot camera{};
    EnvironmentSettings environment{};
    std::vector<RenderObjectSnapshot> objects;
    std::vector<RenderLightSnapshot> lights;
};

/** Builds a render snapshot using the supplied viewport aspect ratio. */
RenderSceneSnapshot ExtractRenderScene(const Scene& scene, f32 aspect);

} // namespace Concord

#endif // CONCORD_RENDERSCENESNAPSHOT_H
