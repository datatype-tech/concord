// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_BOX_H
#define CONCORD_BOX_H

#include "engine/core/Transform.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"
#include "engine/scene/Material.h"

namespace Concord::Object {

/** Every field a Box can be spawned from. */
struct BoxDesc {
    /** Position, rotation and scale. */
    Transform transform{};

    /** Extent along each axis, before `transform.scale` applies. */
    Vec3 size{1.0f, 1.0f, 1.0f};

    /** Surface appearance. */
    Material material{};

    /** Whether the box is drawn at all. */
    bool visible = true;

    /** Whether the box contributes to shadow maps. */
    bool castShadow = true;
};

/**
 * Box primitive.
 *
 * The type carries no state of its own: it is the recipe telling Spawn which
 * components make up a box, plus the archetype label used when querying.
 * The spawned entity's data lives in the World, so the object-oriented and
 * data-oriented views can never drift apart.
 */
struct Box {
    using Desc = BoxDesc;

    /** Attaches the components that constitute a box. */
    static void Build(World& world, Entity entity, const BoxDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<MeshRenderer>(entity, MeshRenderer{
            .shape = PrimitiveShape::Box,
            .size = desc.size,
            .visible = desc.visible,
            .castShadow = desc.castShadow,
        });
        world.Add<Material>(entity, desc.material);
    }
};

} // namespace Concord::Object

#endif // CONCORD_BOX_H
