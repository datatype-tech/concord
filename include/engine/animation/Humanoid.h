// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_HUMANOID_H
#define CONCORD_HUMANOID_H

#include "Concord/CExport.h"
#include "engine/asset/Skeleton.h"
#include "engine/core/Types.h"

#include <array>
#include <string>
#include <string_view>

namespace Concord {

/** Semantic humanoid bone slots the retargeter maps between rigs. */
enum class HumanoidBone {
    Hips,
    Spine,
    Spine1,
    Spine2,
    Neck,
    Head,
    LeftShoulder,
    LeftArm,
    LeftForeArm,
    LeftHand,
    RightShoulder,
    RightArm,
    RightForeArm,
    RightHand,
    LeftUpLeg,
    LeftLeg,
    LeftFoot,
    RightUpLeg,
    RightLeg,
    RightFoot,
};

inline constexpr u32 kHumanoidBoneCount = static_cast<u32>(HumanoidBone::RightFoot) + 1;

/** Maps humanoid bone slots onto the joints of one skeleton. */
struct CENGINE_API HumanoidSkeleton {
    /** The mapped skeleton; must outlive this mapping. */
    const Skeleton* skeleton = nullptr;
    /** Joint index per slot, kInvalidJoint where the rig has no match. */
    std::array<u32, kHumanoidBoneCount> bones{};

    /** Joint index of a semantic slot, or kInvalidJoint when unmapped. */
    [[nodiscard]] u32 Bone(HumanoidBone bone) const noexcept;
    /** Joint index of a raw slot index, or kInvalidJoint when out of range. */
    [[nodiscard]] u32 Bone(u32 slot) const noexcept;
    /** Whether retargeting can run: a valid skeleton carrying every required slot. */
    [[nodiscard]] bool IsValid() const noexcept;
};

/**
 * Strips vendor prefixes and separator variants from a joint name.
 *
 * "mixamorig:Spine_01", "Armature|mixamorig:Spine_01" and "spine1" all
 * normalize to "spine1": everything after the last ':' and '|', lowercased,
 * without '_', '-', ' ' and '.', with leading zeros of a trailing digit run
 * collapsed.
 */
[[nodiscard]] CENGINE_API std::string NormalizeJointName(std::string_view name);

/** Maps a skeleton's joints into humanoid slots by normalized joint names. */
[[nodiscard]] CENGINE_API HumanoidSkeleton BuildHumanoidSkeleton(
    const Skeleton& skeleton) noexcept;

} // namespace Concord

#endif // CONCORD_HUMANOID_H
