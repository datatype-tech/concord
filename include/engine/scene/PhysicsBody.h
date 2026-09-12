// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PHYSICSBODY_H
#define CONCORD_PHYSICSBODY_H

#include "engine/core/Transform.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/World.h"
#include "engine/scene/Material.h"

namespace Concord::Object {

/** An invisible static volume. Use when the mesh already exists on another entity. */
struct StaticBodyDesc {
    Transform transform{};
    CollisionShape shape = CollisionShape::Box;
    Vec3 size{1.0f, 1.0f, 1.0f};
    Vec3 offset{};
    bool isTrigger = false;
};

struct StaticBody {
    using Desc = StaticBodyDesc;

    static void Build(World& world, Entity entity, const StaticBodyDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<Collider>(entity, Collider{.shape = desc.shape,
                                             .size = desc.size,
                                             .offset = desc.offset,
                                             .isTrigger = desc.isTrigger});
        world.Add<RigidBody>(entity, RigidBody{.motion = BodyMotion::Static});
    }
};

/** A drawable box the solver is allowed to throw around. */
struct DynamicBoxDesc {
    Transform transform{};
    Vec3 size{1.0f, 1.0f, 1.0f};
    Material material{};
    f32 mass = 1.0f;
    f32 restitution = 0.08f;
    f32 friction = 0.50f;
    bool lockRotation = false;
    bool visible = true;
    bool castShadow = true;
};

struct DynamicBox {
    using Desc = DynamicBoxDesc;

    static void Build(World& world, Entity entity, const DynamicBoxDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<MeshRenderer>(entity, MeshRenderer{
            .shape = PrimitiveShape::Box,
            .size = desc.size,
            .visible = desc.visible,
            .castShadow = desc.castShadow,
        });
        world.Add<Material>(entity, desc.material);
        world.Add<Collider>(entity, Collider{.shape = CollisionShape::Box, .size = desc.size});
        world.Add<RigidBody>(entity, RigidBody{.motion = BodyMotion::Dynamic,
                                               .mass = desc.mass,
                                               .friction = desc.friction,
                                               .restitution = desc.restitution,
                                               .lockRotation = desc.lockRotation});
    }
};

/** A drawable sphere the solver is allowed to throw around. */
struct DynamicSphereDesc {
    Transform transform{};
    /** Full diameter, matching MeshRenderer.size.x for a sphere. */
    f32 diameter = 1.0f;
    Material material{};
    f32 mass = 1.0f;
    f32 restitution = 0.35f;
    f32 friction = 0.40f;
    bool visible = true;
    bool castShadow = true;
};

struct DynamicSphere {
    using Desc = DynamicSphereDesc;

    static void Build(World& world, Entity entity, const DynamicSphereDesc& desc)
    {
        const Vec3 size{desc.diameter, desc.diameter, desc.diameter};
        world.Add<Transform>(entity, desc.transform);
        world.Add<MeshRenderer>(entity, MeshRenderer{
            .shape = PrimitiveShape::Sphere,
            .size = size,
            .visible = desc.visible,
            .castShadow = desc.castShadow,
        });
        world.Add<Material>(entity, desc.material);
        world.Add<Collider>(entity, Collider{.shape = CollisionShape::Sphere, .size = size});
        world.Add<RigidBody>(entity, RigidBody{.motion = BodyMotion::Dynamic,
                                               .mass = desc.mass,
                                               .friction = desc.friction,
                                               .restitution = desc.restitution});
    }
};

} // namespace Concord::Object

#endif // CONCORD_PHYSICSBODY_H
