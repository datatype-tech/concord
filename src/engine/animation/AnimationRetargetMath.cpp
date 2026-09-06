// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationRetargetInternal.h"

#include <vector>

namespace Concord {
namespace {

Vec3 BindTranslation(const std::vector<Mat4>& globals, u32 joint) noexcept
{
    const Vec4& column = globals[joint].col[3];
    return {column.x, column.y, column.z};
}

/** Bind-pose bone-chain length from `from` down to `to`, or 0 when unrelated. */
f32 ChainLength(const Skeleton& skeleton, const std::vector<Mat4>& globals, u32 from, u32 to)
{
    f32 length = 0.0f;
    i32 current = static_cast<i32>(to);
    usize guard = 0;
    while (current >= 0 && static_cast<usize>(current) < skeleton.joints.size() &&
           guard++ <= skeleton.joints.size()) {
        if (static_cast<u32>(current) == from) return length;
        const i32 parent = skeleton.joints[current].parent;
        if (parent < 0 || static_cast<usize>(parent) >= skeleton.joints.size()) break;
        length += Length(BindTranslation(globals, static_cast<u32>(current)) -
                         BindTranslation(globals, static_cast<u32>(parent)));
        current = parent;
    }
    return 0.0f;
}

} // namespace

void CollectBindGlobals(const Skeleton& skeleton, std::vector<Mat4>& globals)
{
    globals.assign(skeleton.joints.size(), Mat4::Identity());
    std::vector<u8> state(skeleton.joints.size(), 0);
    const auto visit = [&](auto&& self, usize index) -> void {
        if (index >= globals.size() || state[index] == 2) return;
        if (state[index] == 1) {
            state[index] = 2;
            return;
        }
        state[index] = 1;
        const i32 parent = skeleton.joints[index].parent;
        const Mat4 between = index < skeleton.externalRootTransforms.size()
                                 ? skeleton.externalRootTransforms[index]
                                 : Mat4::Identity();
        const Mat4 local = skeleton.joints[index].local.ToMatrix();
        if (parent >= 0 && static_cast<usize>(parent) < globals.size()) {
            self(self, static_cast<usize>(parent));
            globals[index] = globals[static_cast<usize>(parent)] * between * local;
        } else {
            globals[index] = between * local;
        }
        state[index] = 2;
    };
    for (usize index = 0; index < globals.size(); ++index) {
        visit(visit, index);
    }
}

f32 ComputeMotionScale(const HumanoidSkeleton& source, const HumanoidSkeleton& target,
                       const std::vector<Mat4>& sourceGlobals,
                       const std::vector<Mat4>& targetGlobals)
{
    struct Chain {
        HumanoidBone from;
        HumanoidBone to;
    };
    constexpr Chain kChains[] = {
        {HumanoidBone::Hips, HumanoidBone::Head},
        {HumanoidBone::Hips, HumanoidBone::LeftFoot},
        {HumanoidBone::Hips, HumanoidBone::RightFoot},
    };
    f32 total = 0.0f;
    u32 counted = 0;
    for (const Chain& chain : kChains) {
        const u32 sourceFrom = source.Bone(chain.from);
        const u32 sourceTo = source.Bone(chain.to);
        const u32 targetFrom = target.Bone(chain.from);
        const u32 targetTo = target.Bone(chain.to);
        if (sourceFrom == kInvalidJoint || sourceTo == kInvalidJoint ||
            targetFrom == kInvalidJoint || targetTo == kInvalidJoint) {
            continue;
        }
        const f32 sourceLength =
            ChainLength(*source.skeleton, sourceGlobals, sourceFrom, sourceTo);
        const f32 targetLength =
            ChainLength(*target.skeleton, targetGlobals, targetFrom, targetTo);
        if (sourceLength < 0.0001f || targetLength < 0.0001f) continue;
        total += targetLength / sourceLength;
        ++counted;
    }
    return counted > 0 ? total / static_cast<f32>(counted) : 1.0f;
}

void ScaleTranslationChannel(AnimationChannel& channel, f32 scale) noexcept
{
    for (AnimationVec3Key& key : channel.vec3Keys) {
        key.value *= scale;
        key.inTangent *= scale;
        key.outTangent *= scale;
    }
}

void StripHorizontalTranslation(AnimationChannel& channel) noexcept
{
    for (AnimationVec3Key& key : channel.vec3Keys) {
        key.value.x = 0.0f;
        key.value.z = 0.0f;
        key.inTangent.x = 0.0f;
        key.inTangent.z = 0.0f;
        key.outTangent.x = 0.0f;
        key.outTangent.z = 0.0f;
    }
}

void KeepHorizontalTranslation(AnimationChannel& channel, f32 scale) noexcept
{
    for (AnimationVec3Key& key : channel.vec3Keys) {
        key.value = {key.value.x * scale, 0.0f, key.value.z * scale};
        key.inTangent = {key.inTangent.x * scale, 0.0f, key.inTangent.z * scale};
        key.outTangent = {key.outTangent.x * scale, 0.0f, key.outTangent.z * scale};
    }
}

} // namespace Concord
