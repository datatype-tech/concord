// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationRetarget.h"
#include "engine/animation/Humanoid.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.001f;
}

bool SameVec(const Concord::Vec3& left, const Concord::Vec3& right)
{
    return Near(left.x, right.x) && Near(left.y, right.y) && Near(left.z, right.z);
}

Concord::Skeleton MakeRig(std::vector<Concord::Joint> joints)
{
    Concord::Skeleton skeleton{};
    skeleton.joints = std::move(joints);
    skeleton.nodeIndices.resize(skeleton.joints.size());
    for (Concord::u32 index = 0; index < skeleton.joints.size(); ++index) {
        skeleton.nodeIndices[index] = index;
        if (skeleton.joints[index].parent < 0) skeleton.root = static_cast<Concord::i32>(index);
    }
    return skeleton;
}

Concord::Skeleton MakeSourceRig()
{
    return MakeRig({
        {.name = "mixamorig:Hips", .parent = -1, .local = {.translation = {0.0f, 1.0f, 0.0f}}},
        {.name = "mixamorig:Spine", .parent = 0, .local = {.translation = {0.0f, 0.5f, 0.0f}}},
        {.name = "mixamorig:Head", .parent = 1, .local = {.translation = {0.0f, 0.5f, 0.0f}}},
        {.name = "mixamorig:LeftUpLeg", .parent = 0, .local = {.translation = {0.3f, 0.0f, 0.0f}}},
        {.name = "mixamorig:LeftLeg", .parent = 3, .local = {.translation = {0.0f, -0.5f, 0.0f}}},
        {.name = "mixamorig:RightUpLeg", .parent = 0, .local = {.translation = {-0.3f, 0.0f, 0.0f}}},
        {.name = "mixamorig:RightLeg", .parent = 5, .local = {.translation = {0.0f, -0.5f, 0.0f}}},
        {.name = "mixamorig:LeftArm", .parent = 0, .local = {.translation = {0.4f, 0.2f, 0.0f}}},
        {.name = "mixamorig:RightArm", .parent = 0, .local = {.translation = {-0.4f, 0.2f, 0.0f}}},
    });
}

/** Double-sized rig with shuffled joint order and unprefixed names. */
Concord::Skeleton MakeTargetRig()
{
    return MakeRig({
        {.name = "Head", .parent = 1, .local = {.translation = {0.0f, 1.0f, 0.0f}}},
        {.name = "Spine", .parent = 3, .local = {.translation = {0.0f, 1.0f, 0.0f}}},
        {.name = "RightLeg", .parent = 5, .local = {.translation = {0.0f, -1.0f, 0.0f}}},
        {.name = "Hips", .parent = -1, .local = {.translation = {0.0f, 2.0f, 0.0f}}},
        {.name = "LeftLeg", .parent = 6, .local = {.translation = {0.0f, -1.0f, 0.0f}}},
        {.name = "RightUpLeg", .parent = 3, .local = {.translation = {-0.6f, 0.0f, 0.0f}}},
        {.name = "LeftUpLeg", .parent = 3, .local = {.translation = {0.6f, 0.0f, 0.0f}}},
        {.name = "LeftArm", .parent = 3, .local = {.translation = {0.8f, 0.4f, 0.0f}}},
        {.name = "RightArm", .parent = 3, .local = {.translation = {-0.8f, 0.4f, 0.0f}}},
    });
}

Concord::AnimationClip MakeClip()
{
    Concord::AnimationClip clip{};
    clip.name = "Walk";
    clip.duration = 1.0f;
    clip.channels.push_back({.joint = Concord::kInvalidJoint,
                             .path = Concord::AnimationPath::Translation,
                             .vec3Keys = {{.time = 0.0f, .value = {0.0f, 0.0f, 0.0f}},
                                          {.time = 1.0f, .value = {10.0f, 1.0f, 0.0f}}},
                             .sourceNode = 0});
    clip.channels.push_back({.path = Concord::AnimationPath::Rotation,
                             .rotationKeys = {{.time = 0.0f, .value = {0.0f, 0.0f, 0.7071f, 0.7071f}}},
                             .sourceNode = 1});
    clip.channels.push_back({.path = Concord::AnimationPath::Translation,
                             .vec3Keys = {{.time = 0.0f, .value = {0.0f, 0.0f, 0.0f}},
                                          {.time = 1.0f, .value = {1.0f, 0.0f, 0.0f}}},
                             .sourceNode = 4});
    clip.channels.push_back({.path = Concord::AnimationPath::Rotation,
                             .rotationKeys = {{.time = 0.0f, .value = {0.0f, 0.0f, 0.0f, 1.0f}}},
                             .sourceNode = 0});
    return clip;
}

const Concord::AnimationChannel* FindChannel(const Concord::AnimationClip& clip,
                                             Concord::AnimationPath path, Concord::u32 joint)
{
    for (const Concord::AnimationChannel& channel : clip.channels) {
        if (channel.path == path && channel.joint == joint) return &channel;
    }
    return nullptr;
}

bool TestNormalizeJointName()
{
    const std::string mixed = Concord::NormalizeJointName("mixamorig:Spine_01");
    const std::string piped = Concord::NormalizeJointName("Armature|mixamorig:LeftForeArm");
    const std::string plain = Concord::NormalizeJointName("Hips");
    const std::string padded = Concord::NormalizeJointName("spine_010");
    return mixed == "spine1" && piped == "leftforearm" && plain == "hips" &&
           padded == "spine10";
}

bool TestBuildHumanoid()
{
    const Concord::Skeleton source = MakeSourceRig();
    const Concord::HumanoidSkeleton humanoid = Concord::BuildHumanoidSkeleton(source);
    if (!humanoid.IsValid()) {
        std::printf("  humanoid invalid: hips=%u spine=%u head=%u lleg=%u rleg=%u\n",
                    humanoid.Bone(Concord::HumanoidBone::Hips),
                    humanoid.Bone(Concord::HumanoidBone::Spine),
                    humanoid.Bone(Concord::HumanoidBone::Head),
                    humanoid.Bone(Concord::HumanoidBone::LeftUpLeg),
                    humanoid.Bone(Concord::HumanoidBone::RightUpLeg));
        return false;
    }
    if (humanoid.Bone(Concord::HumanoidBone::Hips) != 0 ||
        humanoid.Bone(Concord::HumanoidBone::Head) != 2 ||
        humanoid.Bone(Concord::HumanoidBone::RightLeg) != 6) {
        std::printf("  humanoid slots: hips=%u head=%u rleg=%u\n",
                    humanoid.Bone(Concord::HumanoidBone::Hips),
                    humanoid.Bone(Concord::HumanoidBone::Head),
                    humanoid.Bone(Concord::HumanoidBone::RightLeg));
        return false;
    }

    Concord::Skeleton incomplete = MakeSourceRig();
    incomplete.joints.resize(2);
    incomplete.nodeIndices.resize(2);
    incomplete.root = 0;
    return !Concord::BuildHumanoidSkeleton(incomplete).IsValid();
}

bool TestRetargetMapping()
{
    const Concord::Skeleton source = MakeSourceRig();
    const Concord::Skeleton target = MakeTargetRig();
    const Concord::HumanoidSkeleton sourceHumanoid = Concord::BuildHumanoidSkeleton(source);
    const Concord::HumanoidSkeleton targetHumanoid = Concord::BuildHumanoidSkeleton(target);
    const Concord::AnimationClip clip = MakeClip();

    Concord::RetargetResult result;
    if (!Concord::RetargetClip(sourceHumanoid, clip, targetHumanoid, {}, result)) return false;
    if (result.hasRootMotion || result.clip.channels.size() != 4) return false;

    const Concord::AnimationChannel* hips = FindChannel(
        result.clip, Concord::AnimationPath::Translation,
        targetHumanoid.Bone(Concord::HumanoidBone::Hips));
    if (hips == nullptr || hips->sourceNode != Concord::kInvalidJoint) return false;
    if (!SameVec(hips->vec3Keys.back().value, {20.0f, 2.0f, 0.0f})) return false;

    const Concord::AnimationChannel* leg = FindChannel(
        result.clip, Concord::AnimationPath::Translation,
        targetHumanoid.Bone(Concord::HumanoidBone::LeftLeg));
    if (leg == nullptr || !SameVec(leg->vec3Keys.back().value, {1.0f, 0.0f, 0.0f})) return false;

    const Concord::AnimationChannel* spine = FindChannel(
        result.clip, Concord::AnimationPath::Rotation,
        targetHumanoid.Bone(Concord::HumanoidBone::Spine));
    return spine != nullptr && Near(spine->rotationKeys.front().value.w, 0.7071f);
}

bool TestRootMotionModes()
{
    const Concord::Skeleton source = MakeSourceRig();
    const Concord::Skeleton target = MakeTargetRig();
    const Concord::HumanoidSkeleton sourceHumanoid = Concord::BuildHumanoidSkeleton(source);
    const Concord::HumanoidSkeleton targetHumanoid = Concord::BuildHumanoidSkeleton(target);
    const Concord::AnimationClip clip = MakeClip();
    const Concord::u32 targetHips = targetHumanoid.Bone(Concord::HumanoidBone::Hips);

    Concord::RetargetResult baked;
    Concord::RetargetOptions keep;
    if (!Concord::RetargetClip(sourceHumanoid, clip, targetHumanoid, keep, baked)) return false;
    if (baked.hasRootMotion) return false;

    Concord::RetargetResult inPlace;
    Concord::RetargetOptions place{.rootMotion = Concord::RootMotionMode::InPlace};
    if (!Concord::RetargetClip(sourceHumanoid, clip, targetHumanoid, place, inPlace)) return false;
    const Concord::AnimationChannel* held = FindChannel(
        inPlace.clip, Concord::AnimationPath::Translation, targetHips);
    if (held == nullptr || !SameVec(held->vec3Keys.back().value, {0.0f, 2.0f, 0.0f})) return false;

    Concord::RetargetResult extracted;
    Concord::RetargetOptions extract{.rootMotion = Concord::RootMotionMode::ExtractRootMotion};
    if (!Concord::RetargetClip(sourceHumanoid, clip, targetHumanoid, extract, extracted)) {
        return false;
    }
    if (!extracted.hasRootMotion || extracted.rootMotion.name != "Walk_RootMotion") return false;
    const Concord::AnimationChannel* rootMotion = FindChannel(
        extracted.rootMotion, Concord::AnimationPath::Translation, targetHips);
    return rootMotion != nullptr &&
           SameVec(rootMotion->vec3Keys.back().value, {20.0f, 0.0f, 0.0f}) &&
           FindChannel(extracted.rootMotion, Concord::AnimationPath::Rotation, targetHips) !=
               nullptr;
}

bool TestInvalidHumanoidFails()
{
    Concord::Skeleton incomplete = MakeSourceRig();
    incomplete.joints.resize(2);
    incomplete.nodeIndices.resize(2);
    incomplete.root = 0;
    const Concord::Skeleton complete = MakeTargetRig();
    const Concord::AnimationClip clip = MakeClip();
    Concord::RetargetResult result;
    return !Concord::RetargetClip(Concord::BuildHumanoidSkeleton(incomplete), clip,
                                  Concord::BuildHumanoidSkeleton(complete), {}, result);
}

} // namespace

int main()
{
    struct Case {
        const char* name;
        bool (*run)();
    };
    const Case cases[] = {
        {"normalize-names", TestNormalizeJointName},
        {"build-humanoid", TestBuildHumanoid},
        {"retarget-mapping", TestRetargetMapping},
        {"root-motion-modes", TestRootMotionModes},
        {"invalid-humanoid", TestInvalidHumanoidFails},
    };
    for (const Case& testCase : cases) {
        if (!testCase.run()) {
            std::printf("FAIL %s\n", testCase.name);
            return 1;
        }
    }
    return 0;
}
