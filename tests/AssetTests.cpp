// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"
#include "Concord/CModel.h"

#include <cmath>
#include <string>

namespace {

bool Near(float a, float b) noexcept { return std::abs(a - b) < 0.0001f; }

bool TestObj()
{
    const auto result = Concord::ModelLoader::LoadObj(
        "o triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    const auto groups = Concord::ModelLoader::LoadObj(
        "o one\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\no two\nv 0 0 1\nv 1 0 1\nv 0 1 1\nf 4 5 6\n");
    const auto malformed = Concord::ModelLoader::LoadObj("v 0 0 0 trailing\n");
    return result.Succeeded() && groups.Succeeded() && groups.asset.meshes.size() == 2 &&
           !malformed.Succeeded() && result.asset.meshes.size() == 1 &&
           result.asset.meshes[0].primitives.size() == 1 &&
           Near(result.asset.meshes[0].primitives[0].vertices[0].normal.z, 1.0f);
}

bool TestGltf()
{
    const std::string json = R"json({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAABAAIA"}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"meshes":[{"name":"triangle","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"nodes":[{"mesh":0}]})json";
    const auto result = Concord::ModelLoader::LoadGltf(json);
    return result.Succeeded() && result.asset.meshes.size() == 1 &&
           result.asset.meshes[0].primitives[0].indices.size() == 3;
}

bool TestGltfAnimationMapping()
{
    const std::string json = R"json({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":76,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAABAAIAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAA=="}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":8},{"buffer":0,"byteOffset":52,"byteLength":24}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":3,"componentType":5126,"count":2,"type":"VEC3"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"nodes":[{"mesh":0,"skin":0},{"name":"helper"},{"name":"root","children":[3]},{"name":"animated"}],
"skins":[{"name":"rig-a","joints":[2,3],"skeleton":2},{"name":"rig-b","joints":[3,2],"skeleton":2}],
"animations":[{"name":"walk","samplers":[{"input":2,"output":3}],"channels":[{"sampler":0,"target":{"node":3,"path":"translation"}}]}]})json";
    const auto result = Concord::ModelLoader::LoadGltf(json);
    if (!result.Succeeded() || result.asset.skeletons.size() != 2 || result.asset.animations.size() != 1) return false;
    const auto& first = result.asset.skeletons[0];
    const auto& second = result.asset.skeletons[1];
    if (first.FindJoint(3) != 1 || second.FindJoint(3) != 0) return false;
    const auto* imported = result.asset.FindAnimation("walk");
    if (!imported || imported->channels.size() != 1 || imported->channels[0].sourceNode != 3 || imported->channels[0].joint != Concord::kInvalidJoint) return false;
    Concord::AnimationClip clip = *imported;
    clip.channels[0].joint = 0;
    auto firstPose = first.CreateBindPose();
    auto secondPose = second.CreateBindPose();
    return Concord::SampleAnimation(first, clip, 0.5f, firstPose, false) &&
           Concord::SampleAnimation(second, clip, 0.5f, secondPose, false) &&
           Near(firstPose.local[1].translation.y, 1.0f) &&
           Near(secondPose.local[0].translation.y, 1.0f);
}

bool TestSkeletonAndAnimation()
{
    Concord::Skeleton skeleton{.name = "ordered", .joints = {
        Concord::Joint{.name = "child", .parent = 1, .local = {.translation = {0.0f, 3.0f, 0.0f}}},
        Concord::Joint{.name = "root", .parent = -1, .local = {.translation = {2.0f, 0.0f, 0.0f}}}}, .root = 1};
    if (!skeleton.IsValid()) return false;
    auto pose = skeleton.CreateBindPose();
    if (!Near(pose.jointMatrices[0].col[3].x, 2.0f) || !Near(pose.jointMatrices[0].col[3].y, 3.0f)) return false;
    Concord::AnimationClip clip{.name = "move", .duration = 1.0f, .channels = {
        Concord::AnimationChannel{.joint = 0, .path = Concord::AnimationPath::Translation,
            .vec3Keys = {{.time = 0.0f, .value = {0.0f, 3.0f, 0.0f}}, {.time = 1.0f, .value = {0.0f, 5.0f, 0.0f}}}}}};
    Concord::AnimationPlayer player;
    player.Play(&clip, false);
    if (!player.Update(skeleton, 0.5f, pose) || !Near(pose.local[0].translation.y, 4.0f)) return false;
    player.Stop();
    if (!player.Update(skeleton, 0.5f, pose) || !Near(pose.local[0].translation.y, 4.0f)) return false;
    return true;
}

} // namespace

int main()
{
    return TestObj() && TestGltf() && TestGltfAnimationMapping() && TestSkeletonAndAnimation() ? 0 : 1;
}
