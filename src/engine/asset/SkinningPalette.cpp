// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/SkinningPalette.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

bool IsFinite(const Mat4& matrix) noexcept
{
    for (const Vec4& column : matrix.col) {
        for (u32 component = 0; component < 4; ++component) {
            if (!std::isfinite(column[component])) return false;
        }
    }
    return true;
}

Mat4 SafeMatrix(const Mat4& matrix) noexcept
{
    return IsFinite(matrix) ? matrix : Mat4::Identity();
}

SkinningPaletteRange EmptyRange(usize first, u32 flags) noexcept
{
    const usize bounded = std::min(first, static_cast<usize>(kMaxSkinningPaletteJoints));
    return {.firstJoint = static_cast<u32>(bounded), .jointCount = 0, .flags = flags};
}

} // namespace

SkinningPaletteRange AppendSkinningPalette(SkinningPaletteUpload& upload,
                                           std::span<const Mat4> matrices) noexcept
{
    const usize first = upload.jointMatrices.size();
    if (first >= kMaxSkinningPaletteJoints) {
        return EmptyRange(first, kSkinningPaletteOverflow | kSkinningPaletteTruncated);
    }
    const usize available = kMaxSkinningPaletteJoints - first;
    const usize count = std::min({matrices.size(), available,
                                  static_cast<usize>(kMaxSkinningJoints)});
    u32 flags = matrices.size() > count ? kSkinningPaletteTruncated : 0u;
    try {
        upload.jointMatrices.reserve(first + count);
        for (usize index = 0; index < count; ++index) {
            upload.jointMatrices.push_back(SafeMatrix(matrices[index]));
        }
    } catch (...) {
        upload.jointMatrices.resize(first);
        return EmptyRange(first, flags | kSkinningPaletteOverflow);
    }
    return {.firstJoint = static_cast<u32>(first),
            .jointCount = static_cast<u32>(count),
            .flags = flags};
}

SkinningPaletteRange AppendSkinningPalette(SkinningPaletteUpload& upload,
                                           const SkeletonPose& pose) noexcept
{
    return AppendSkinningPalette(upload, std::span<const Mat4>(pose.jointMatrices));
}

void ClearSkinningPalette(SkinningPaletteUpload& upload) noexcept
{
    upload.jointMatrices.clear();
}

std::span<const std::byte> SkinningPaletteBytes(const SkinningPaletteUpload& upload) noexcept
{
    return std::as_bytes(std::span<const Mat4>(upload.jointMatrices));
}

} // namespace Concord
