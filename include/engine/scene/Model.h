// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_MODEL_H
#define CONCORD_MODEL_H

#include "engine/core/Transform.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"
#include "engine/scene/ModelRenderer.h"

#include <memory>

namespace Concord::Object {

/** Every field needed to spawn an imported model instance. */
struct ModelDesc {
    /** Decoded immutable model asset to reference. */
    std::shared_ptr<const ModelAsset> asset{};
    /** World transform applied to the selected meshes. */
    Transform transform{};
    /** Mesh to draw, or all meshes when left at its default. */
    u32 meshIndex = kAllModelMeshes;
    /** Whether the instance is visible. */
    bool visible = true;
    /** Whether the instance casts shadows and RT geometry. */
    bool castShadow = true;
};

/** Imported model archetype; geometry remains owned by the shared asset. */
struct Model {
    using Desc = ModelDesc;

    /** Attaches transform and model-renderer components to an entity. */
    static void Build(World& world, Entity entity, const ModelDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<ModelRenderer>(entity, ModelRenderer{
            .asset = desc.asset,
            .meshIndex = desc.meshIndex,
            .visible = desc.visible,
            .castShadow = desc.castShadow,
        });
    }
};

} // namespace Concord::Object

#endif // CONCORD_MODEL_H
