// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/Animation.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

f32 SafeTime(f32 time, f32 duration, bool loop) noexcept
{
    if (!std::isfinite(time) || !std::isfinite(duration) || duration <= 0.0f) {
        return 0.0f;
    }
    if (loop) {
        time = std::fmod(time, duration);
        return time < 0.0f ? time + duration : time;
    }
    return std::clamp(time, 0.0f, duration);
}

template <typename Key>
usize Segment(const std::vector<Key>& keys, f32 time) noexcept
{
    if (keys.size() < 2) {
        return 0;
    }
    const auto it = std::upper_bound(keys.begin(), keys.end(), time,
                                     [](f32 value, const Key& key) { return value < key.time; });
    return static_cast<usize>(std::distance(keys.begin(), it == keys.begin() ? it : it - 1));
}

Vec3 Cubic(const AnimationVec3Key& a, const AnimationVec3Key& b, f32 amount) noexcept
{
    const f32 dt = std::max(b.time - a.time, 0.0f);
    const f32 t2 = amount * amount;
    const f32 t3 = t2 * amount;
    const f32 h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const f32 h10 = t3 - 2.0f * t2 + amount;
    const f32 h01 = -2.0f * t3 + 3.0f * t2;
    const f32 h11 = t3 - t2;
    return a.value * h00 + a.outTangent * (h10 * dt) + b.value * h01 + b.inTangent * (h11 * dt);
}

Quat Cubic(const AnimationQuatKey& a, const AnimationQuatKey& b, f32 amount) noexcept
{
    const f32 dt = std::max(b.time - a.time, 0.0f);
    const f32 t2 = amount * amount, t3 = t2 * amount;
    const f32 h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const f32 h10 = t3 - 2.0f * t2 + amount;
    const f32 h01 = -2.0f * t3 + 3.0f * t2;
    const f32 h11 = t3 - t2;
    return Quat{a.value.x * h00 + a.outTangent.x * h10 * dt + b.value.x * h01 + b.inTangent.x * h11 * dt,
                a.value.y * h00 + a.outTangent.y * h10 * dt + b.value.y * h01 + b.inTangent.y * h11 * dt,
                a.value.z * h00 + a.outTangent.z * h10 * dt + b.value.z * h01 + b.inTangent.z * h11 * dt,
                a.value.w * h00 + a.outTangent.w * h10 * dt + b.value.w * h01 + b.inTangent.w * h11 * dt}
        .Normalized();
}

Vec3 SampleVec3(const std::vector<AnimationVec3Key>& keys, AnimationInterpolation mode,
                f32 time) noexcept
{
    if (keys.empty()) return {};
    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;
    const usize index = Segment(keys, time);
    const auto& a = keys[index]; const auto& b = keys[index + 1];
    const f32 amount = b.time > a.time ? (time - a.time) / (b.time - a.time) : 0.0f;
    if (mode == AnimationInterpolation::Step) return a.value;
    return mode == AnimationInterpolation::CubicSpline ? Cubic(a, b, amount)
                                                       : a.value * (1.0f - amount) + b.value * amount;
}

Quat SampleQuat(const std::vector<AnimationQuatKey>& keys, AnimationInterpolation mode,
                f32 time) noexcept
{
    if (keys.empty()) return Quat::Identity();
    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value.Normalized();
    if (time >= keys.back().time) return keys.back().value.Normalized();
    const usize index = Segment(keys, time);
    const auto& a = keys[index]; const auto& b = keys[index + 1];
    const f32 amount = b.time > a.time ? (time - a.time) / (b.time - a.time) : 0.0f;
    if (mode == AnimationInterpolation::Step) return a.value.Normalized();
    return mode == AnimationInterpolation::CubicSpline ? Cubic(a, b, amount)
                                                       : Slerp(a.value, b.value, amount);
}

u32 ResolveJoint(const Skeleton& skeleton, const AnimationChannel& channel) noexcept
{
    if (channel.sourceNode != kInvalidJoint) return skeleton.FindJoint(channel.sourceNode);
    return channel.joint;
}

} // namespace

bool SampleAnimation(const Skeleton& skeleton, const AnimationClip& clip, f32 time,
                     SkeletonPose& pose, bool loop) noexcept
{
    try {
        pose.Reset(skeleton);
        if (!skeleton.IsValid()) return false;
        const f32 sampleTime = SafeTime(time, clip.duration, loop);
        for (const AnimationChannel& channel : clip.channels) {
            const u32 joint = ResolveJoint(skeleton, channel);
            if (joint == kInvalidJoint || joint >= pose.local.size()) continue;
            BoneTransform& target = pose.local[joint];
            if (channel.path == AnimationPath::Rotation) {
                if (channel.rotationKeys.empty()) continue;
                target.rotation = SampleQuat(channel.rotationKeys, channel.interpolation, sampleTime);
            } else {
                if (channel.vec3Keys.empty()) continue;
                const Vec3 value = SampleVec3(channel.vec3Keys, channel.interpolation, sampleTime);
                if (channel.path == AnimationPath::Translation) target.translation = value;
                if (channel.path == AnimationPath::Scale) target.scale = value;
            }
        }
        skeleton.BuildJointMatrices(pose.local, pose.jointMatrices);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Concord
