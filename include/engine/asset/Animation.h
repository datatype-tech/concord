// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_ANIMATION_H
#define CONCORD_ASSET_ANIMATION_H

#include "Concord/CExport.h"
#include "engine/asset/Skeleton.h"

#include <string>
#include <vector>

namespace Concord {

/** Property animated by one channel. */
enum class AnimationPath { Translation, Rotation, Scale };

/** Key interpolation mode from the glTF animation specification. */
enum class AnimationInterpolation { Step, Linear, CubicSpline };

/** A vector key; tangent fields are used for cubic-spline channels. */
struct AnimationVec3Key {
    f32 time = 0.0f;
    Vec3 value{};
    Vec3 inTangent{};
    Vec3 outTangent{};
};

/** A quaternion key; tangent fields are used for cubic-spline channels. */
struct AnimationQuatKey {
    f32 time = 0.0f;
    Quat value{};
    Quat inTangent{};
    Quat outTangent{};
};

/** One joint property track in an animation clip. */
struct AnimationChannel {
    u32 joint = kInvalidJoint;
    AnimationPath path = AnimationPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<AnimationVec3Key> vec3Keys;
    std::vector<AnimationQuatKey> rotationKeys;
    /** Original glTF node index; used to resolve the joint per skeleton. */
    u32 sourceNode = kInvalidJoint;
};

/** A named animation clip with local-space joint channels. */
struct CENGINE_API AnimationClip {
    std::string name;
    f32 duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

/** Samples a clip into a pose, starting from the skeleton's bind pose. */
CENGINE_API bool SampleAnimation(const Skeleton& skeleton,
                                 const AnimationClip& clip,
                                 f32 time,
                                 SkeletonPose& pose,
                                 bool loop = true) noexcept;

/** Stateful clip player suitable for an ECS update system. */
class CENGINE_API AnimationPlayer {
public:
    /** Selects a clip and resets playback time. */
    void Play(const AnimationClip* clip, bool loop = true) noexcept;
    /** Stops playback while retaining the current sampled pose. */
    void Stop() noexcept { m_playing = false; }
    /** Advances time and samples the selected clip. */
    bool Update(const Skeleton& skeleton, f32 deltaSeconds, SkeletonPose& pose) noexcept;
    /** Current clip time in seconds. */
    [[nodiscard]] f32 Time() const noexcept { return m_time; }
    /** Whether a clip is selected and advancing. */
    [[nodiscard]] bool IsPlaying() const noexcept { return m_playing && m_clip != nullptr; }

private:
    const AnimationClip* m_clip = nullptr;
    f32 m_time = 0.0f;
    bool m_loop = true;
    bool m_playing = false;
};

} // namespace Concord

#endif // CONCORD_ASSET_ANIMATION_H
