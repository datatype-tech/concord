// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/Humanoid.h"

#include <algorithm>

namespace Concord {
namespace {

struct AliasEntry {
    std::string_view name;
    HumanoidBone bone;
};

/** Normalized joint-name aliases per semantic slot, first match wins. */
constexpr AliasEntry kAliases[] = {
    {"hips", HumanoidBone::Hips},
    {"spine", HumanoidBone::Spine},
    {"spine1", HumanoidBone::Spine1},
    {"spine2", HumanoidBone::Spine2},
    {"chest", HumanoidBone::Spine2},
    {"neck", HumanoidBone::Neck},
    {"head", HumanoidBone::Head},
    {"leftshoulder", HumanoidBone::LeftShoulder},
    {"leftarm", HumanoidBone::LeftArm},
    {"leftforearm", HumanoidBone::LeftForeArm},
    {"leftelbow", HumanoidBone::LeftForeArm},
    {"lefthand", HumanoidBone::LeftHand},
    {"leftwrist", HumanoidBone::LeftHand},
    {"rightshoulder", HumanoidBone::RightShoulder},
    {"rightarm", HumanoidBone::RightArm},
    {"rightforearm", HumanoidBone::RightForeArm},
    {"rightelbow", HumanoidBone::RightForeArm},
    {"righthand", HumanoidBone::RightHand},
    {"rightwrist", HumanoidBone::RightHand},
    {"leftupleg", HumanoidBone::LeftUpLeg},
    {"leftthigh", HumanoidBone::LeftUpLeg},
    {"leftleg", HumanoidBone::LeftLeg},
    {"leftknee", HumanoidBone::LeftLeg},
    {"leftfoot", HumanoidBone::LeftFoot},
    {"leftankle", HumanoidBone::LeftFoot},
    {"rightupleg", HumanoidBone::RightUpLeg},
    {"rightthigh", HumanoidBone::RightUpLeg},
    {"rightleg", HumanoidBone::RightLeg},
    {"rightknee", HumanoidBone::RightLeg},
    {"rightfoot", HumanoidBone::RightFoot},
    {"rightankle", HumanoidBone::RightFoot},
};

std::string Normalize(std::string_view name)
{
    std::string_view trimmed = name;
    const usize colon = trimmed.find_last_of(':');
    if (colon != std::string_view::npos) trimmed = trimmed.substr(colon + 1);
    const usize pipe = trimmed.find_last_of('|');
    if (pipe != std::string_view::npos) trimmed = trimmed.substr(pipe + 1);

    std::string result;
    result.reserve(trimmed.size());
    for (const char character : trimmed) {
        if (character == '_' || character == '-' || character == ' ' || character == '.') {
            continue;
        }
        result.push_back(static_cast<char>(
            character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character));
    }

    usize digitsEnd = result.size();
    while (digitsEnd > 0 && result[digitsEnd - 1] >= '0' && result[digitsEnd - 1] <= '9') {
        --digitsEnd;
    }
    usize firstDigit = digitsEnd;
    while (firstDigit + 1 < result.size() && result[firstDigit] == '0') {
        ++firstDigit;
    }
    if (firstDigit != digitsEnd) {
        result.erase(digitsEnd, firstDigit - digitsEnd);
    }
    return result;
}

} // namespace

std::string NormalizeJointName(std::string_view name) { return Normalize(name); }

u32 HumanoidSkeleton::Bone(HumanoidBone bone) const noexcept
{
    return bones[static_cast<usize>(bone)];
}

u32 HumanoidSkeleton::Bone(u32 slot) const noexcept
{
    return slot < kHumanoidBoneCount ? bones[slot] : kInvalidJoint;
}

bool HumanoidSkeleton::IsValid() const noexcept
{
    return skeleton != nullptr && skeleton->IsValid() &&
           Bone(HumanoidBone::Hips) != kInvalidJoint &&
           Bone(HumanoidBone::Spine) != kInvalidJoint &&
           Bone(HumanoidBone::LeftArm) != kInvalidJoint &&
           Bone(HumanoidBone::RightArm) != kInvalidJoint &&
           Bone(HumanoidBone::LeftUpLeg) != kInvalidJoint &&
           Bone(HumanoidBone::RightUpLeg) != kInvalidJoint;
}

HumanoidSkeleton BuildHumanoidSkeleton(const Skeleton& skeleton) noexcept
{
    HumanoidSkeleton humanoid{};
    humanoid.skeleton = &skeleton;
    humanoid.bones.fill(kInvalidJoint);
    for (u32 joint = 0; joint < skeleton.joints.size(); ++joint) {
        const std::string normalized = NormalizeJointName(skeleton.joints[joint].name);
        for (const AliasEntry& entry : kAliases) {
            if (entry.name == std::string_view(normalized) &&
                humanoid.bones[static_cast<usize>(entry.bone)] == kInvalidJoint) {
                humanoid.bones[static_cast<usize>(entry.bone)] = joint;
                break;
            }
        }
    }
    return humanoid;
}

} // namespace Concord
