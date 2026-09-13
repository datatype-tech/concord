// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/SkinnedGeometry.h"

#include "engine/asset/SkinningPalette.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Keeps a non-finite or non-positive weight out of the blend. */
f32 SafeWeight(f32 value) noexcept
{
    return std::isfinite(value) && value > 0.0f ? value : 0.0f;
}

/** Linear part of one matrix column, read as a direction. */
Vec3 Column(const Mat4& matrix, u32 column) noexcept
{
    return {matrix.col[column].x, matrix.col[column].y, matrix.col[column].z};
}

/** Transforms an affine point, ignoring the homogeneous divide. */
Vec3 TransformPoint(const Mat4& matrix, Vec3 point) noexcept
{
    const Vec4 result = matrix * Vec4{point.x, point.y, point.z, 1.0f};
    return {result.x, result.y, result.z};
}

/**
 * Applies the inverse-transpose of the linear part of the matrix.
 *
 * Skinning matrices carry the inverse bind pose, so they scale and shear as
 * well as rotate; rebuilding the normal this way keeps it perpendicular to
 * the deformed surface instead of merely rotated. For a matrix whose columns
 * are a, b and c, the rows of its inverse are the cross products below, so
 * the inverse-transpose is the matrix holding them as columns.
 */
Vec3 TransformNormal(const Mat4& matrix, Vec3 normal) noexcept
{
    const Vec3 a = Column(matrix, 0);
    const Vec3 b = Column(matrix, 1);
    const Vec3 c = Column(matrix, 2);
    const Vec3 row0 = Cross(b, c);
    const Vec3 row1 = Cross(c, a);
    const Vec3 row2 = Cross(a, b);
    const f32 determinant = Dot(a, row0);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f) {
        return normal;
    }
    return {(row0.x * normal.x + row1.x * normal.y + row2.x * normal.z) / determinant,
            (row0.y * normal.x + row1.y * normal.y + row2.y * normal.z) / determinant,
            (row0.z * normal.x + row1.z * normal.y + row2.z * normal.z) / determinant};
}

/** Blends the usable palette slice into one vertex skinning matrix. */
bool BuildSkinMatrix(const ModelVertex& vertex, std::span<const Mat4> joints,
                     u32 firstJoint, u32 availableJoints, Mat4& skin) noexcept
{
    const f32 weights[4] = {SafeWeight(vertex.weights.x), SafeWeight(vertex.weights.y),
                            SafeWeight(vertex.weights.z), SafeWeight(vertex.weights.w)};
    const f32 total = weights[0] + weights[1] + weights[2] + weights[3];
    if (!(total > 1e-5f)) {
        return false;
    }
    skin = Mat4{};
    for (u32 influence = 0; influence < 4; ++influence) {
        if (weights[influence] <= 0.0f) {
            continue;
        }
        const u32 joint = std::min<u32>(vertex.joints[influence], availableJoints - 1);
        const Mat4& matrix = joints[firstJoint + joint];
        const f32 weight = weights[influence] / total;
        for (u32 column = 0; column < 4; ++column) {
            for (u32 row = 0; row < 4; ++row) {
                skin.col[column][row] += matrix.col[column][row] * weight;
            }
        }
    }
    return true;
}

/** Resolves how many palette matrices this primitive may legally address. */
u32 AvailableJoints(std::span<const Mat4> joints, u32 firstJoint, u32 jointCount) noexcept
{
    if (firstJoint >= joints.size() || jointCount == 0) {
        return 0;
    }
    const usize remaining = joints.size() - firstJoint;
    return static_cast<u32>(std::min<usize>(
        static_cast<usize>(std::min<u32>(jointCount, kMaxSkinningJoints)), remaining));
}

} // namespace

bool SolveSkinnedVertices(const ModelPrimitive& primitive, std::span<const Mat4> joints,
                          u32 firstJoint, u32 jointCount,
                          std::span<SkinnedVertex> output) noexcept
{
    if (output.size() < primitive.vertices.size()) {
        return false;
    }
    const u32 availableJoints = AvailableJoints(joints, firstJoint, jointCount);
    for (usize index = 0; index < primitive.vertices.size(); ++index) {
        const ModelVertex& vertex = primitive.vertices[index];
        SkinnedVertex& result = output[index];
        result.position = vertex.position;
        result.normal = vertex.normal;
        if (availableJoints == 0) {
            continue;
        }
        Mat4 skin{};
        if (!BuildSkinMatrix(vertex, joints, firstJoint, availableJoints, skin)) {
            continue;
        }
        const Vec3 position = TransformPoint(skin, vertex.position);
        if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
            !std::isfinite(position.z)) {
            continue;
        }
        result.position = position;
        const Vec3 deformed = TransformNormal(skin, vertex.normal);
        const f32 length = Length(deformed);
        if (std::isfinite(length) && length > 1e-6f) {
            result.normal = deformed / length;
        }
    }
    return true;
}

} // namespace Concord
