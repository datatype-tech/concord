// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_SKINNEDGEOMETRY_H
#define CONCORD_ASSET_SKINNEDGEOMETRY_H

#include "Concord/CExport.h"
#include "engine/asset/ModelAsset.h"
#include "engine/core/Mat4.h"
#include "engine/core/Vec3.h"

#include <span>

namespace Concord {

/** One linear-blend-skinned vertex, in the mesh's own local space. */
struct SkinnedVertex {
    Vec3 position{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
};

/**
 * Deforms a rest-pose primitive with a joint palette slice.
 *
 * Applies exactly the rule the GPU skinning vertex shader applies, so a
 * CPU-deformed copy and a rasterized skinned draw agree vertex for vertex:
 * joint indices clamp into the available range, non-finite and non-positive
 * weights are dropped, the surviving weights are normalized, and a vertex
 * whose weights are all unusable keeps its rest pose.
 *
 * The result is expressed in the space the primitive was authored in, so the
 * caller still applies the object matrix afterwards.
 *
 * @param primitive Rest-pose geometry whose joint indices address the palette.
 * @param joints Mesh-space skinning matrices, as staged by SkinningPalette.
 * @param firstJoint First usable matrix in the palette span.
 * @param jointCount Number of matrices this primitive may address.
 * @param output Destination with room for every vertex of the primitive.
 * @return false when the output span is too small for the primitive.
 */
[[nodiscard]] CENGINE_API bool SolveSkinnedVertices(
    const ModelPrimitive& primitive, std::span<const Mat4> joints,
    u32 firstJoint, u32 jointCount, std::span<SkinnedVertex> output) noexcept;

} // namespace Concord

#endif // CONCORD_ASSET_SKINNEDGEOMETRY_H
