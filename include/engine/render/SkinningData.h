// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDER_SKINNINGDATA_H
#define CONCORD_RENDER_SKINNINGDATA_H

#include "engine/asset/ModelAsset.h"
#include "engine/asset/SkinningPalette.h"
#include "engine/core/Mat4.h"
#include "engine/core/Vec4.h"

#include <cstddef>

namespace Concord {

/** Descriptor coordinates shared by the skinned graphics pipeline. */
inline constexpr u32 kSkinningPaletteDescriptorSet = 1;
inline constexpr u32 kSkinningPaletteBinding = 0;
inline constexpr u32 kSkinningTextureDescriptorSet = 2;

/** Vertex attribute locations used by ModelVertex in the skinned shader. */
inline constexpr u32 kSkinningPositionLocation = 0;
inline constexpr u32 kSkinningNormalLocation = 1;
inline constexpr u32 kSkinningTangentLocation = 2;
inline constexpr u32 kSkinningTexcoordLocation = 3;
inline constexpr u32 kSkinningJointsLocation = 4;
inline constexpr u32 kSkinningWeightsLocation = 5;

/** Byte layout of ModelVertex attributes used by the skinned pipeline. */
inline constexpr usize kSkinningVertexStride = sizeof(ModelVertex);
inline constexpr usize kSkinningPositionOffset = offsetof(ModelVertex, position);
inline constexpr usize kSkinningNormalOffset = offsetof(ModelVertex, normal);
inline constexpr usize kSkinningTangentOffset = offsetof(ModelVertex, tangent);
inline constexpr usize kSkinningTexcoordOffset = offsetof(ModelVertex, texcoord);
inline constexpr usize kSkinningJointsOffset = offsetof(ModelVertex, joints);
inline constexpr usize kSkinningWeightsOffset = offsetof(ModelVertex, weights);

/** Push constants shared by a future skinned forward pipeline. */
struct alignas(16) SkinningObjectPushConstants {
    /** Object-to-world transform applied after skinning. */
    Mat4 model = Mat4::Identity();
    /** Linear-space base color, including alpha in w. */
    Vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    /** x metallic, y roughness, z emissive strength. */
    Vec4 material{0.0f, 0.8f, 0.0f, 0.0f};
    /** Matrix range in the frame skinning SSBO. */
    SkinningPaletteRange palette{};
};

static_assert(sizeof(SkinningObjectPushConstants) == 112);
static_assert(offsetof(SkinningObjectPushConstants, palette) == 96);

} // namespace Concord

#endif // CONCORD_RENDER_SKINNINGDATA_H
