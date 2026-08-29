// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/Skeleton.h"

#include <algorithm>
#include <cmath>

namespace Concord {

Mat4 BoneTransform::ToMatrix() const noexcept
{
    return Mat4::Translate(translation) * rotation.ToMatrix() * Mat4::Scale(scale);
}

BoneTransform BoneTransform::Interpolate(const BoneTransform& a,
                                         const BoneTransform& b,
                                         f32 amount) noexcept
{
    amount = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
    return {
        .translation = a.translation * (1.0f - amount) + b.translation * amount,
        .rotation = Slerp(a.rotation, b.rotation, amount),
        .scale = a.scale * (1.0f - amount) + b.scale * amount,
    };
}

void SkeletonPose::Reset(const Skeleton& skeleton)
{
    local.clear();
    local.reserve(skeleton.joints.size());
    for (const Joint& joint : skeleton.joints) {
        local.push_back(joint.local);
    }
    jointMatrices.resize(skeleton.joints.size(), Mat4::Identity());
    skeleton.BuildJointMatrices(local, jointMatrices);
}

bool Skeleton::IsValid() const noexcept
{
    if (joints.empty()) {
        return root == -1;
    }
    if (root < 0 || static_cast<usize>(root) >= joints.size()) {
        return false;
    }
    usize roots = 0;
    for (usize index = 0; index < joints.size(); ++index) {
        const i32 parent = joints[index].parent;
        if (parent < -1 || (parent >= 0 && static_cast<usize>(parent) >= joints.size()) || parent == static_cast<i32>(index)) {
            return false;
        }
        if (parent < 0) ++roots;
        const auto finite = [](const Mat4& matrix) {
            for (const Vec4& column : matrix.col) {
                if (!std::isfinite(column.x) || !std::isfinite(column.y) ||
                    !std::isfinite(column.z) || !std::isfinite(column.w)) return false;
            }
            return true;
        };
        if (!finite(joints[index].inverseBind)) return false;
        const BoneTransform& transform = joints[index].local;
        if (!std::isfinite(transform.translation.x) || !std::isfinite(transform.translation.y) ||
            !std::isfinite(transform.translation.z) || !std::isfinite(transform.scale.x) ||
            !std::isfinite(transform.scale.y) || !std::isfinite(transform.scale.z) ||
            !std::isfinite(transform.rotation.x) || !std::isfinite(transform.rotation.y) ||
            !std::isfinite(transform.rotation.z) || !std::isfinite(transform.rotation.w)) return false;
    }
    if (roots == 0 || joints[static_cast<usize>(root)].parent != -1) return false;
    if (!nodeIndices.empty()) {
        if (nodeIndices.size() != joints.size()) return false;
        for (usize index = 0; index < nodeIndices.size(); ++index) {
            for (usize other = 0; other < index; ++other) {
                if (nodeIndices[index] == nodeIndices[other]) return false;
            }
        }
    }
    for (usize start = 0; start < joints.size(); ++start) {
        std::vector<u8> seen(joints.size(), 0);
        i32 current = static_cast<i32>(start);
        while (current >= 0) {
            const usize index = static_cast<usize>(current);
            if (seen[index] != 0) return false;
            seen[index] = 1;
            current = joints[index].parent;
        }
    }
    return true;
}

u32 Skeleton::FindJoint(u32 nodeIndex) const noexcept
{
    if (nodeIndices.size() != joints.size()) return kInvalidJoint;
    for (usize joint = 0; joint < nodeIndices.size(); ++joint) {
        if (nodeIndices[joint] == nodeIndex) return static_cast<u32>(joint);
    }
    return kInvalidJoint;
}

void Skeleton::BuildJointMatrices(std::span<const BoneTransform> local,
                                  std::span<Mat4> output) const noexcept
{
    const usize count = std::min({joints.size(), local.size(), output.size()});
    std::vector<Mat4> globals(count, Mat4::Identity());
    std::vector<u8> state(count, 0);
    const auto visit = [&](auto&& self, usize index) -> void {
        if (index >= count || state[index] == 2) return;
        if (state[index] == 1) { state[index] = 2; return; }
        state[index] = 1;
        const i32 parent = joints[index].parent;
        if (parent >= 0 && static_cast<usize>(parent) < count) self(self, static_cast<usize>(parent));
        const Mat4 parentMatrix = parent >= 0 && static_cast<usize>(parent) < count
                                      ? globals[static_cast<usize>(parent)] : Mat4::Identity();
        globals[index] = parentMatrix * local[index].ToMatrix();
        output[index] = globals[index] * joints[index].inverseBind;
        state[index] = 2;
    };
    for (usize index = 0; index < count; ++index) visit(visit, index);
}

SkeletonPose Skeleton::CreateBindPose() const
{
    SkeletonPose pose;
    pose.Reset(*this);
    return pose;
}

} // namespace Concord
