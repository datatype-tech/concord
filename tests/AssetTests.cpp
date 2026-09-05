// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"
#include "Concord/CModel.h"
#include "engine/asset/ImageAsset.h"

#include <cmath>
#include <limits>
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

bool TestGltfEmbeddedImage()
{
    const std::string json = R"json({
"asset":{"version":"2.0"},
"buffers":[
 {"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAABAAIA"},
 {"byteLength":74,"uri":"data:application/octet-stream;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAAEUlEQVR4nGP4z8DQwPCf4T8ADn0Dfur2k8AAAAAASUVORK5CYII="}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":1,"byteLength":74}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"images":[{"bufferView":2,"mimeType":"image/png"}],
"textures":[{"source":0}],
"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
"nodes":[{"mesh":0}]})json";
    const auto result = Concord::ModelLoader::LoadGltf(json);
    if (!result.Succeeded() || result.asset.materials.size() != 1) return false;
    const std::string& uri = result.asset.materials[0].baseColorTexture;
    if (uri.rfind("data:image/png;base64,", 0) != 0) return false;
    const auto image = Concord::ImageLoader::LoadUri(uri);
    return image.Succeeded() && image.image.width == 2 && image.image.height == 1;
}

bool TestGltfAnimationMapping()
{
    const std::string json = R"json({
"asset":{"version":"2.0"},
"buffers":[{"byteLength":76,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAABAAIAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAA=="}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},{"buffer":0,"byteOffset":44,"byteLength":8},{"buffer":0,"byteOffset":52,"byteLength":24}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":3,"componentType":5126,"count":2,"type":"VEC3"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"nodes":[{"mesh":0,"skin":0},{"name":"helper","translation":[3,0,0],"children":[2]},{"name":"root","children":[4]},{"name":"animated"},{"name":"joint-helper","translation":[0,2,0],"children":[3]}],
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
    if (!Near(firstPose.jointMatrices[0].col[3].x, 3.0f) ||
        !Near(firstPose.jointMatrices[1].col[3].x, 3.0f) ||
        !Near(firstPose.jointMatrices[1].col[3].y, 2.0f) ||
        !Near(secondPose.jointMatrices[1].col[3].x, 3.0f) ||
        !Near(secondPose.jointMatrices[0].col[3].y, 2.0f)) return false;
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

    player.Play(&clip, false);
    if (!player.Update(skeleton, 0.75f, pose)) return false;
    if (!player.Update(skeleton, -1.0f, pose) || player.Time() != 0.0f || player.IsPlaying()) return false;

    player.Play(&clip, true);
    const float largeDelta = std::numeric_limits<float>::max();
    if (!player.Update(skeleton, largeDelta, pose) ||
        !player.Update(skeleton, largeDelta, pose) ||
        !std::isfinite(player.Time()) || player.Time() < 0.0f || player.Time() >= clip.duration) return false;
    player.Play(&clip, true);
    if (!player.Update(skeleton, 1.25f, pose) || !Near(player.Time(), 0.25f)) return false;
    return true;
}

bool TestRejectsMalformedNodeTransform()
{
    Concord::ModelAsset asset{};
    asset.materials.push_back(Concord::ModelMaterial{});
    Concord::ModelPrimitive primitive{};
    primitive.vertices = {Concord::ModelVertex{.position = {0.0f, 0.0f, 0.0f}},
                          Concord::ModelVertex{.position = {1.0f, 0.0f, 0.0f}},
                          Concord::ModelVertex{.position = {0.0f, 1.0f, 0.0f}}};
    primitive.indices = {0, 1, 2};
    asset.meshes.push_back(Concord::ModelMesh{.primitives = {primitive}});
    asset.nodes.push_back(Concord::ModelNode{.mesh = 0});
    asset.nodes[0].local.translation.x = std::numeric_limits<float>::quiet_NaN();
    return !asset.IsValid();
}

} // namespace

int main()
{
    return TestObj() && TestGltf() && TestGltfEmbeddedImage() && TestGltfAnimationMapping() && TestSkeletonAndAnimation() &&
           TestRejectsMalformedNodeTransform() ? 0 : 1;
}
