// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_SKINNINGPALETTE_H
#define CONCORD_ASSET_SKINNINGPALETTE_H

#include "Concord/CExport.h"
#include "engine/asset/Skeleton.h"
#include "engine/core/Mat4.h"
#include "engine/core/Types.h"

#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace Concord {

/** Maximum number of joints addressable by one skinned draw. */
inline constexpr u32 kMaxSkinningJoints = 256;

/** Maximum number of matrices staged in one frame palette. */
inline constexpr u32 kMaxSkinningPaletteJoints = 65536;

/** Flags describing truncation or capacity failure for a palette range. */
inline constexpr u32 kSkinningPaletteTruncated = 1u << 0;
inline constexpr u32 kSkinningPaletteOverflow = 1u << 1;

/** Offset and count consumed by the skinned vertex shader.
 *
 * The range borrows the matrix storage in `SkinningPaletteUpload`; it remains
 * valid until that upload is appended to, cleared, or destroyed.
 */
struct alignas(16) SkinningPaletteRange {
    /** First matrix in the frame's concatenated palette. */
    u32 firstJoint = 0;
    /** Number of matrices available to this draw. */
    u32 jointCount = 0;
    /** Combination of `kSkinningPalette*` status bits. */
    u32 flags = 0;
    /** Reserved for a future palette format revision. */
    u32 reserved = 0;
};

/** CPU staging storage whose matrices map directly to a std430 mat4 array. */
struct SkinningPaletteUpload {
    /** Matrices concatenated in draw order for one frame. */
    std::vector<Mat4> jointMatrices;
};

/** Appends a bounded matrix span and returns its shader-visible range. */
[[nodiscard]] CENGINE_API SkinningPaletteRange AppendSkinningPalette(
    SkinningPaletteUpload& upload, std::span<const Mat4> matrices) noexcept;

/** Appends the derived matrices from a sampled pose. */
[[nodiscard]] CENGINE_API SkinningPaletteRange AppendSkinningPalette(
    SkinningPaletteUpload& upload, const SkeletonPose& pose) noexcept;

/** Removes staged matrices while retaining the upload allocation. */
CENGINE_API void ClearSkinningPalette(SkinningPaletteUpload& upload) noexcept;

/** Returns borrowed bytes ready for a host-visible SSBO upload.
 *
 * The span is invalidated by any operation that changes `upload.jointMatrices`.
 */
[[nodiscard]] CENGINE_API std::span<const std::byte> SkinningPaletteBytes(
    const SkinningPaletteUpload& upload) noexcept;

static_assert(sizeof(SkinningPaletteRange) == 16);
static_assert(sizeof(Mat4) == 64);
static_assert(std::is_standard_layout_v<SkinningPaletteRange>);
static_assert(std::is_trivially_copyable_v<SkinningPaletteRange>);

} // namespace Concord

#endif // CONCORD_ASSET_SKINNINGPALETTE_H
