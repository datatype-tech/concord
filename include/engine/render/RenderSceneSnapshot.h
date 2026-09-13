// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERSCENESNAPSHOT_H
#define CONCORD_RENDERSCENESNAPSHOT_H

#include "engine/core/Mat4.h"
#include "engine/core/Transform.h"
#include "engine/core/Vec2.h"
#include "engine/asset/SkinningPalette.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/scene/EnvironmentSettings.h"
#include "engine/render/RenderParticleSnapshot.h"
#include "engine/scene/Material.h"
#include "engine/scene/ModelRenderer.h"

#include <memory>
#include <vector>

namespace Concord {

inline constexpr u32 kInvalidRenderNode = 0xFFFFFFFFu;

/**
 * Disturbances one frame can hand to the shader.
 *
 * Bounded because the hit shader loops over them for every water pixel: a list
 * that could grow without limit would let a busy scene quietly cost more per
 * pixel than the whole rest of the shading.
 */
inline constexpr u32 kMaxRenderRipples = 64;

/**
 * Water surfaces one frame hands the hit shader for the "wet near water" look.
 *
 * Bounded for the same reason ripples are: every dry fragment tests itself
 * against this list, so its size has to be a constant rather than however
 * many water bodies a scene happens to contain.
 */
inline constexpr u32 kMaxWetnessBodies = 8;

class Scene;

/** Camera data copied from a Scene for one render frame. */
struct RenderCameraSnapshot {
    Entity entity{};
    Vec3 position{};
    /** Unit world-space direction from the camera's Transform orientation. */
    Vec3 forward{0.0f, 0.0f, -1.0f};
    /** Derived point one unit ahead, retained for shadow framing. */
    Vec3 target{0.0f, 0.0f, -1.0f};
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
    /** Imported asset retained for this snapshot's lifetime. */
    std::shared_ptr<const ModelAsset> modelAsset{};
    /** Selected mesh, or kAllModelMeshes for the whole asset. */
    u32 modelMesh = kAllModelMeshes;
    /** Source node that contributed this draw, or kInvalidRenderNode. */
    u32 modelNode = kInvalidRenderNode;
    /** Skin index attached to the source node, or -1 for static geometry. */
    i32 modelSkin = -1;
    /** Palette range for a skinned model, or an empty range for static data. */
    SkinningPaletteRange skinningRange{};
};

/**
 * One live water disturbance, ready to be packed for the hit shader.
 *
 * The strength is already scaled by how far through its life the ripple is, so
 * the shader sees the ring at the amplitude it should be drawn at and never
 * has to know that anything is fading.
 */
struct RenderRippleSnapshot {
    Vec2 centre{};
    f32 wavelength = 3.0f;
    f32 speed = 1.8f;
    f32 strength = 0.4f;
    f32 falloff = 0.18f;
    f32 reach = 24.0f;
    /** Seconds since it was dropped; this is what places the wavefront. */
    f32 age = 0.0f;
};

/**
 * One water surface's footprint, ready to be packed for the hit shader.
 *
 * Approximated as a circle rather than the exact rectangle
 * `WaterBodyComponent` authors: a rendering-only proximity test does not need
 * the precision `WaterSplashSystem`'s collision test does, and a circle costs
 * one scalar per fragment instead of a min/max pair per axis.
 */
struct RenderWaterBodySnapshot {
    Vec2 centre{};
    f32 worldY = 0.0f;
    /** Covers the surface's longer half extent, so the circle never falls short. */
    f32 radius = 0.0f;
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
    /** Concatenated joint matrices referenced by `RenderObjectSnapshot`s. */
    SkinningPaletteUpload skinningPalette{};
    /** Camera-facing billboards expanded from every live particle emitter. */
    RenderParticleSnapshot particles{};
    /** Live water disturbances, newest last and bounded by kMaxRenderRipples. */
    std::vector<RenderRippleSnapshot> ripples;
    /** Water surfaces for the wetness look, bounded by kMaxWetnessBodies. */
    std::vector<RenderWaterBodySnapshot> waterBodies;
};

/** Builds a render snapshot using the supplied viewport aspect ratio. */
RenderSceneSnapshot ExtractRenderScene(const Scene& scene, f32 aspect);

} // namespace Concord

#endif // CONCORD_RENDERSCENESNAPSHOT_H
