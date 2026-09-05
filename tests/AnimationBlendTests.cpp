// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationBlend.h"
#include "engine/animation/AnimationLayer.h"
#include "engine/animation/JointMask.h"

#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.001f;
}

Concord::Skeleton MakeSkeleton()
{
    Concord::Skeleton skeleton{};
    skeleton.joints.push_back(Concord::Joint{.name = "root", .parent = -1});
    skeleton.joints.push_back(
        Concord::Joint{.name = "child", .parent = 0,
                       .local = Concord::BoneTransform{.translation = {1.0f, 0.0f, 0.0f}}});
    skeleton.nodeIndices = {0, 1};
    skeleton.root = 0;
    return skeleton;
}

Concord::SkeletonPose PoseWith(Concord::Skeleton& skeleton,
                               const Concord::BoneTransform& root,
                               const Concord::BoneTransform& child)
{
    Concord::SkeletonPose pose = skeleton.CreateBindPose();
    pose.local[0] = root;
    pose.local[1] = child;
    skeleton.BuildJointMatrices(pose.local, pose.jointMatrices);
    return pose;
}

bool TestBlendWeights()
{
    Concord::Skeleton skeleton = MakeSkeleton();
    const Concord::SkeletonPose from = PoseWith(
        skeleton, Concord::BoneTransform{}, Concord::BoneTransform{});
    const Concord::SkeletonPose to = PoseWith(
        skeleton, Concord::BoneTransform{.translation = {0.0f, 4.0f, 0.0f}},
        Concord::BoneTransform{.translation = {3.0f, 0.0f, 0.0f}});

    Concord::SkeletonPose result;
    if (!Concord::BlendPoses(skeleton, from, to, 0.5f, result)) return false;
    if (!Near(result.local[0].translation.y, 2.0f) ||
        !Near(result.local[1].translation.x, 1.5f) ||
        result.jointMatrices.size() != 2) {
        return false;
    }
    if (!Concord::BlendPoses(skeleton, from, to, 0.0f, result)) return false;
    if (!Near(result.local[0].translation.y, 0.0f)) return false;
    if (!Concord::BlendPoses(skeleton, from, to, 1.0f, result)) return false;
    if (!Near(result.local[0].translation.y, 4.0f)) return false;
    if (!Concord::BlendPoses(skeleton, from, to, NAN, result)) return false;
    if (!Near(result.local[0].translation.y, 0.0f)) return false;
    return true;
}

bool TestBlendRotationSlerp()
{
    const Concord::Quat quarter{0.0f, 0.0f, 0.70710678118f, 0.70710678118f};
    const Concord::BoneTransform blended =
        Concord::BlendBoneTransform(Concord::BoneTransform{}, Concord::BoneTransform{.rotation = quarter},
                                    0.5f);
    const Concord::Mat4 matrix = blended.rotation.ToMatrix();
    return Near(matrix.col[0].x, 0.7071f) && Near(matrix.col[0].y, 0.7071f);
}

bool TestAdditiveBlend()
{
    Concord::Skeleton skeleton = MakeSkeleton();
    const Concord::SkeletonPose bind = skeleton.CreateBindPose();
    Concord::SkeletonPose additive = skeleton.CreateBindPose();
    additive.local[0].translation = {0.0f, 2.0f, 0.0f};
    additive.local[1].translation = {1.0f, 3.0f, 0.0f};

    Concord::SkeletonPose result;
    if (!Concord::AdditiveBlendPose(skeleton, bind, additive, 1.0f, result)) return false;
    return Near(result.local[0].translation.y, 2.0f) &&
           Near(result.local[1].translation.y, 3.0f) &&
           Near(result.local[1].translation.x, 1.0f);
}

bool TestJointMasks()
{
    Concord::Skeleton skeleton = MakeSkeleton();
    const Concord::JointMask subtree = Concord::MaskSubtree(skeleton, 0);
    if (!Near(subtree.Weight(0), 1.0f) || !Near(subtree.Weight(1), 1.0f) ||
        !Near(subtree.Weight(9), 0.0f)) {
        return false;
    }
    const Concord::JointMask childOnly = Concord::MaskSubtree(skeleton, 1);
    if (!Near(childOnly.Weight(0), 0.0f) || !Near(childOnly.Weight(1), 1.0f)) return false;
    const std::vector<std::string_view> childNames{"child"};
    const Concord::JointMask named = Concord::MaskJoints(skeleton, childNames);
    if (!Near(named.Weight(0), 0.0f) || !Near(named.Weight(1), 1.0f)) return false;
    const Concord::JointMask empty;
    if (!Near(empty.Weight(0), 1.0f)) return false;
    return true;
}

bool TestLayerOverrideAndAdditive()
{
    Concord::Skeleton skeleton = MakeSkeleton();
    Concord::SkeletonPose accumulated = PoseWith(
        skeleton, Concord::BoneTransform{}, Concord::BoneTransform{});
    Concord::SkeletonPose layerPose = PoseWith(
        skeleton, Concord::BoneTransform{.translation = {0.0f, 4.0f, 0.0f}},
        Concord::BoneTransform{});

    Concord::AnimationLayer layer{};
    layer.enabled = true;
    layer.mode = Concord::AnimationBlendMode::Override;
    layer.weight = 0.5f;
    if (!Concord::ApplyAnimationLayer(skeleton, layer, layerPose, accumulated)) return false;
    if (!Near(accumulated.local[0].translation.y, 2.0f)) return false;

    accumulated = PoseWith(skeleton, Concord::BoneTransform{}, Concord::BoneTransform{});
    layer.mode = Concord::AnimationBlendMode::Additive;
    layer.weight = 1.0f;
    if (!Concord::ApplyAnimationLayer(skeleton, layer, layerPose, accumulated)) return false;
    if (!Near(accumulated.local[0].translation.y, 4.0f)) return false;

    layer.enabled = false;
    if (!Concord::ApplyAnimationLayer(skeleton, layer, layerPose, accumulated)) return false;
    return Near(accumulated.local[0].translation.y, 4.0f);
}

} // namespace

int main()
{
    struct Case {
        const char* name;
        bool (*run)();
    };
    const Case cases[] = {
        {"blend-weights", TestBlendWeights},
        {"blend-rotation", TestBlendRotationSlerp},
        {"additive-blend", TestAdditiveBlend},
        {"joint-masks", TestJointMasks},
        {"layer-override-additive", TestLayerOverrideAndAdditive},
    };
    for (const Case& testCase : cases) {
        if (!testCase.run()) {
            std::printf("FAIL %s\n", testCase.name);
            return 1;
        }
    }
    return 0;
}
