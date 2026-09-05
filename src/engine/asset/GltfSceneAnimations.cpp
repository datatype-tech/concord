// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <algorithm>

namespace Concord::AssetGltf {
namespace {

AnimationInterpolation Interpolation(std::string_view value) noexcept
{
    if (value == "STEP") return AnimationInterpolation::Step;
    if (value == "CUBICSPLINE") return AnimationInterpolation::CubicSpline;
    return AnimationInterpolation::Linear;
}

bool ReadTimes(const Context& context, i32 accessor, std::vector<f32>& times)
{
    if (!ReadFloatAccessor(context, accessor, 1, times)) return false;
    return std::is_sorted(times.begin(), times.end());
}

bool ReadVecChannel(const Context& context, const AnimationChannel& channel,
                    i32 outputAccessor, const std::vector<f32>& times,
                    AnimationClip& clip)
{
    std::vector<f32> values;
    const u32 components = channel.path == AnimationPath::Rotation ? 4u : 3u;
    if (!ReadFloatAccessor(context, outputAccessor, components, values)) return false;
    const usize multiplier = channel.interpolation == AnimationInterpolation::CubicSpline ? 3u : 1u;
    if (values.size() != times.size() * components * multiplier) return false;
    AnimationChannel result = channel;
    if (channel.path == AnimationPath::Rotation) result.rotationKeys.resize(times.size());
    else result.vec3Keys.resize(times.size());
    for (usize i = 0; i < times.size(); ++i) {
        const usize base = i * components * multiplier;
        if (channel.path == AnimationPath::Rotation) {
            auto& key = result.rotationKeys[i]; key.time = times[i];
            const usize valueBase = base + (multiplier == 3 ? components : 0);
            key.value = {values[valueBase], values[valueBase + 1], values[valueBase + 2], values[valueBase + 3]};
            if (multiplier == 3) { key.inTangent = {values[base], values[base + 1], values[base + 2], values[base + 3]}; key.outTangent = {values[base + 8], values[base + 9], values[base + 10], values[base + 11]}; }
        } else {
            auto& key = result.vec3Keys[i]; key.time = times[i];
            const usize valueBase = base + (multiplier == 3 ? components : 0);
            const f32 animationScale =
                channel.path == AnimationPath::Translation ? context.options.scale : 1.0f;
            key.value = {values[valueBase] * animationScale,
                         values[valueBase + 1] * animationScale,
                         values[valueBase + 2] * animationScale};
            if (multiplier == 3) { key.inTangent = {values[base] * animationScale, values[base + 1] * animationScale, values[base + 2] * animationScale}; key.outTangent = {values[base + 6] * animationScale, values[base + 7] * animationScale, values[base + 8] * animationScale}; }
        }
        clip.duration = std::max(clip.duration, times[i]);
    }
    clip.channels.push_back(std::move(result));
    return true;
}

/** Whether at least one channel resolves to a joint of any imported skeleton. */
bool ClipTargetsSkeleton(const ModelAsset& asset, const AnimationClip& clip) noexcept
{
    for (const AnimationChannel& channel : clip.channels) {
        if (channel.sourceNode == kInvalidJoint && channel.joint == kInvalidJoint) continue;
        for (const Skeleton& skeleton : asset.skeletons) {
            const u32 joint = channel.sourceNode != kInvalidJoint
                                  ? skeleton.FindJoint(channel.sourceNode)
                                  : channel.joint;
            if (joint != kInvalidJoint && joint < skeleton.joints.size()) return true;
        }
    }
    return false;
}

} // namespace

bool ReadAnimations(Context& context)
{
    const AssetJson::Value* animations = Member(*context.root, "animations");
    context.asset.animations.clear();
    if (!animations) return true;
    if (!animations->Is(AssetJson::Type::Array)) return context.Fail("glTF animations must be an array");
    for (const AssetJson::Value& record : animations->array) {
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF animation");
        const auto* samplers = Member(record, "samplers"); const auto* channels = Member(record, "channels");
        if (!samplers || !samplers->Is(AssetJson::Type::Array) || !channels || !channels->Is(AssetJson::Type::Array)) return context.Fail("glTF animation is missing samplers or channels");
        AnimationClip clip{}; clip.name = std::string(Member(record, "name") ? Member(record, "name")->String() : std::string_view{});
        for (const auto& channelRecord : channels->array) {
            if (!channelRecord.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF animation channel");
            i32 samplerIndex = -1, nodeIndex = -1;
            const auto* samplerValue = Member(channelRecord, "sampler"); const auto* target = Member(channelRecord, "target");
            if (!SignedIndex(samplerValue, samplerIndex) || samplerIndex < 0 || static_cast<usize>(samplerIndex) >= samplers->array.size() || !target || !target->Is(AssetJson::Type::Object) || !SignedIndex(Member(*target, "node"), nodeIndex) || nodeIndex < 0 || static_cast<usize>(nodeIndex) >= context.asset.nodes.size()) return context.Fail("invalid glTF animation target");
            const std::string_view path = Member(*target, "path") ? Member(*target, "path")->String() : std::string_view{};
            AnimationPath animationPath = path == "rotation" ? AnimationPath::Rotation : path == "scale" ? AnimationPath::Scale : path == "translation" ? AnimationPath::Translation : AnimationPath::Translation;
            if (path != "rotation" && path != "scale" && path != "translation") return context.Fail("unsupported glTF animation path");
            const auto& sampler = samplers->array[static_cast<usize>(samplerIndex)];
            i32 input = -1, output = -1; if (!SignedIndex(Member(sampler, "input"), input) || !SignedIndex(Member(sampler, "output"), output)) return context.Fail("invalid glTF animation sampler");
            std::vector<f32> times; if (!ReadTimes(context, input, times)) return context.Fail("invalid or unsorted glTF animation times");
            AnimationChannel descriptor{.path = animationPath, .interpolation = Interpolation(Member(sampler, "interpolation") ? Member(sampler, "interpolation")->String("LINEAR") : "LINEAR")};
            descriptor.sourceNode = static_cast<u32>(nodeIndex);
            if (!ReadVecChannel(context, descriptor, output, times, clip)) return context.Fail("invalid glTF animation values");
        }
        if (context.options.strict && !clip.channels.empty() &&
            !ClipTargetsSkeleton(context.asset, clip)) {
            return context.Fail("glTF animation targets no skeleton joints");
        }
        context.asset.animations.push_back(std::move(clip));
    }
    return true;
}

} // namespace Concord::AssetGltf
