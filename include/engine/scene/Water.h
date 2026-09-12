// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATER_H
#define CONCORD_WATER_H

#include "engine/asset/ModelAsset.h"
#include "engine/asset/WaterMaterial.h"
#include "engine/core/Transform.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"
#include "engine/scene/Model.h"
#include "engine/scene/ModelRenderer.h"

#include <memory>

namespace Concord::Object {

/** Every field needed to spawn a body of water. */
struct WaterDesc {
    /** Mesh the surface is drawn on; it is stamped with `surface`. */
    std::shared_ptr<ModelAsset> asset{};
    /** Authored appearance and motion. */
    WaterMaterial surface{};
    /** Position, rotation and scale of the body. */
    Transform transform{};
    /** Mesh to draw, or all meshes when left at its default. */
    u32 meshIndex = kAllModelMeshes;
    /** Whether the instance is visible. */
    bool visible = true;
    /** Whether the body contributes to shadow maps. */
    bool castShadow = true;
};

/**
 * Water archetype.
 *
 * Water is a material rather than a geometry type, so this recipe stamps
 * `surface` onto every material of the asset and then attaches exactly the
 * components `Object::Model` would, leaving a body of water queryable as an
 * ordinary model. The asset arrives mutable because the stamp is what makes it
 * water; a body that owns its asset exclusively is the expected arrangement,
 * since two bodies sharing one asset also share the surface stamped last.
 */
struct Water {
    using Desc = WaterDesc;

    /** Stamps the surface and attaches transform and model renderer. */
    static void Build(World& world, Entity entity, const WaterDesc& desc)
    {
        if (desc.asset) {
            // A meshed asset always carries at least one material; an asset
            // with none would render dry, which the stamp reports.
            (void)ApplyWaterMaterial(*desc.asset, desc.surface);
        }
        Model::Build(world, entity, ModelDesc{
            .asset = desc.asset,
            .transform = desc.transform,
            .meshIndex = desc.meshIndex,
            .visible = desc.visible,
            .castShadow = desc.castShadow,
        });
    }
};

/** Open-water body. Defaults to `MakeOceanSurface()` when `surface` is left unset. */
struct OceanDesc {
    std::shared_ptr<ModelAsset> asset{};
    WaterMaterial surface = MakeOceanSurface();
    Transform transform{};
    u32 meshIndex = kAllModelMeshes;
    bool visible = true;
    bool castShadow = true;
};

struct Ocean {
    using Desc = OceanDesc;

    static void Build(World& world, Entity entity, const OceanDesc& desc)
    {
        Water::Build(world, entity,
                     WaterDesc{.asset = desc.asset,
                               .surface = desc.surface,
                               .transform = desc.transform,
                               .meshIndex = desc.meshIndex,
                               .visible = desc.visible,
                               .castShadow = desc.castShadow});
    }
};

/** Flowing channel. Defaults to `MakeRiverWater()`. */
struct RiverDesc {
    std::shared_ptr<ModelAsset> asset{};
    WaterMaterial surface = MakeRiverWater();
    Transform transform{};
    u32 meshIndex = kAllModelMeshes;
    bool visible = true;
    bool castShadow = true;
};

struct River {
    using Desc = RiverDesc;

    static void Build(World& world, Entity entity, const RiverDesc& desc)
    {
        Water::Build(world, entity,
                     WaterDesc{.asset = desc.asset,
                               .surface = desc.surface,
                               .transform = desc.transform,
                               .meshIndex = desc.meshIndex,
                               .visible = desc.visible,
                               .castShadow = desc.castShadow});
    }
};

/** Falling sheet. Flow is evaluated on the mesh UV, so the card needs TEXCOORD_0. */
struct WaterfallDesc {
    std::shared_ptr<ModelAsset> asset{};
    WaterMaterial surface = MakeWaterfall();
    Transform transform{};
    u32 meshIndex = kAllModelMeshes;
    bool visible = true;
    bool castShadow = true;
};

struct Waterfall {
    using Desc = WaterfallDesc;

    static void Build(World& world, Entity entity, const WaterfallDesc& desc)
    {
        Water::Build(world, entity,
                     WaterDesc{.asset = desc.asset,
                               .surface = desc.surface,
                               .transform = desc.transform,
                               .meshIndex = desc.meshIndex,
                               .visible = desc.visible,
                               .castShadow = desc.castShadow});
    }
};

} // namespace Concord::Object

#endif // CONCORD_WATER_H
