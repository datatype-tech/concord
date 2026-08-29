// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_SKELETON_H
#define CONCORD_ASSET_SKELETON_H

#include "Concord/CExport.h"
#include "engine/core/Mat4.h"
#include "engine/core/Quat.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"

#include <span>
#include <string>
#include <vector>

namespace Concord {

inline constexpr u32 kInvalidJoint = 0xFFFFFFFFu;

/** Local translation, rotation and scale of a node or joint. */
struct BoneTransform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    /** Converts local TRS to the matrix used by skinning and node traversal. */
    [[nodiscard]] CENGINE_API Mat4 ToMatrix() const noexcept;
    /** Interpolates two local transforms while preserving quaternion winding. */
    [[nodiscard]] CENGINE_API static BoneTransform Interpolate(const BoneTransform& a,
                                                               const BoneTransform& b,
                                                               f32 amount) noexcept;
};

/** One named joint in a glTF-compatible bind hierarchy. */
struct Joint {
    std::string name;
    i32 parent = -1;
    BoneTransform local{};
    Mat4 inverseBind = Mat4::Identity();
};

/** Mutable local and derived global pose for one skeleton. */
struct SkeletonPose {
    std::vector<BoneTransform> local;
    std::vector<Mat4> jointMatrices;

    /** Resizes and initializes a pose from the skeleton's bind transforms. */
    CENGINE_API void Reset(const struct Skeleton& skeleton);
};

/** A validated hierarchy and inverse-bind matrices used by skinned meshes. */
struct CENGINE_API Skeleton {
    std::string name;
    std::vector<Joint> joints;
    /** Source glTF node index corresponding to each joint. */
    std::vector<u32> nodeIndices;
    i32 root = -1;

    /** Returns false when parent links or inverse-bind dimensions are invalid. */
    [[nodiscard]] bool IsValid() const noexcept;
    /** Resolves a source glTF node index to this skeleton's joint index. */
    [[nodiscard]] u32 FindJoint(u32 nodeIndex) const noexcept;
    /** Computes global skin matrices from local pose values. */
    void BuildJointMatrices(std::span<const BoneTransform> local,
                            std::span<Mat4> output) const noexcept;
    /** Initializes a pose and computes its bind skin matrices. */
    [[nodiscard]] SkeletonPose CreateBindPose() const;
};

} // namespace Concord

#endif // CONCORD_ASSET_SKELETON_H
