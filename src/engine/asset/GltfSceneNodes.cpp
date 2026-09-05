// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace Concord::AssetGltf {
namespace {

bool VecValues(const AssetJson::Value* array, u32 count, f32* output) noexcept
{
    if (!array || !array->Is(AssetJson::Type::Array) || array->array.size() < count) return false;
    for (u32 i = 0; i < count; ++i) if (!FloatValue(&array->array[i], output[i])) return false;
    return true;
}

Quat MatrixRotation(const f32* m, Vec3 scale) noexcept
{
    const f32 r00 = m[0] / scale.x, r11 = m[5] / scale.y, r22 = m[10] / scale.z;
    const f32 r01 = m[4] / scale.y, r02 = m[8] / scale.z, r10 = m[1] / scale.x;
    const f32 r12 = m[9] / scale.z, r20 = m[2] / scale.x, r21 = m[6] / scale.y;
    const f32 trace = r00 + r11 + r22;
    Quat q{};
    if (trace > 0.0f) {
        const f32 s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {(r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s, 0.25f * s};
    } else if (r00 > r11 && r00 > r22) {
        const f32 s = std::sqrt(std::max(1.0f + r00 - r11 - r22, 0.0f)) * 2.0f;
        q = {0.25f * s, (r01 + r10) / s, (r02 + r20) / s, (r21 - r12) / s};
    } else if (r11 > r22) {
        const f32 s = std::sqrt(std::max(1.0f + r11 - r00 - r22, 0.0f)) * 2.0f;
        q = {(r01 + r10) / s, 0.25f * s, (r12 + r21) / s, (r02 - r20) / s};
    } else {
        const f32 s = std::sqrt(std::max(1.0f + r22 - r00 - r11, 0.0f)) * 2.0f;
        q = {(r02 + r20) / s, (r12 + r21) / s, 0.25f * s, (r10 - r01) / s};
    }
    return q.Normalized();
}

bool ReadNodeTransform(Context& context, const AssetJson::Value& record, BoneTransform& transform)
{
    f32 values[16]{};
    if (const auto* matrix = Member(record, "matrix")) {
        if (!VecValues(matrix, 16, values)) return false;
        transform.translation = {values[12], values[13], values[14]};
        transform.scale = {Length({values[0], values[1], values[2]}), Length({values[4], values[5], values[6]}), Length({values[8], values[9], values[10]})};
        if (transform.scale.x < 0.000001f || transform.scale.y < 0.000001f || transform.scale.z < 0.000001f) return false;
        const f32 determinant = values[0] * (values[5] * values[10] - values[6] * values[9]) -
                                values[4] * (values[1] * values[10] - values[2] * values[9]) +
                                values[8] * (values[1] * values[6] - values[2] * values[5]);
        if (determinant < 0.0f) {
            // A mirrored matrix has no quaternion representation; decomposing one
            // would silently produce a wrong rotation, so reject or strip it.
            if (context.options.strict) return context.Fail("glTF node matrix has negative scale");
            transform.rotation = Quat::Identity();
            return true;
        }
        transform.rotation = MatrixRotation(values, transform.scale);
        return true;
    }
    if (const auto* translation = Member(record, "translation")) {
        f32 v[3]{}; if (!VecValues(translation, 3, v)) return false; transform.translation = {v[0], v[1], v[2]};
    }
    if (const auto* rotation = Member(record, "rotation")) {
        f32 v[4]{}; if (!VecValues(rotation, 4, v)) return false; transform.rotation = Quat{v[0], v[1], v[2], v[3]}.Normalized();
    }
    if (const auto* scale = Member(record, "scale")) {
        f32 v[3]{}; if (!VecValues(scale, 3, v)) return false; transform.scale = {v[0], v[1], v[2]};
    }
    return true;
}

} // namespace

bool ReadNodes(Context& context)
{
    const AssetJson::Value* nodes = Member(*context.root, "nodes");
    context.asset.nodes.clear();
    if (!nodes) return true;
    if (!nodes->Is(AssetJson::Type::Array)) return context.Fail("glTF nodes must be an array");
    context.asset.nodes.resize(nodes->array.size());
    for (usize i = 0; i < nodes->array.size(); ++i) {
        const AssetJson::Value& record = nodes->array[i];
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF node");
        ModelNode& node = context.asset.nodes[i];
        node.name = std::string(Member(record, "name") ? Member(record, "name")->String() : std::string_view{});
        if (!ReadNodeTransform(context, record, node.local)) return context.Fail("invalid glTF node transform");
        node.local.translation *= context.options.scale;
        if (Member(record, "mesh") && (!SignedIndex(Member(record, "mesh"), node.mesh) || node.mesh < 0 || static_cast<usize>(node.mesh) >= context.asset.meshes.size())) return context.Fail("glTF node mesh index out of range");
        if (Member(record, "skin") && (!SignedIndex(Member(record, "skin"), node.skin) || node.skin < 0)) return context.Fail("invalid glTF node skin index");
        const AssetJson::Value* children = Member(record, "children");
        if (children) {
            if (!children->Is(AssetJson::Type::Array)) return context.Fail("glTF node children must be an array");
            for (const auto& child : children->array) { usize childIndex = 0; if (!IndexValue(&child, childIndex) || childIndex >= nodes->array.size()) return context.Fail("glTF child index out of range"); node.children.push_back(static_cast<u32>(childIndex)); }
        }
    }
    for (usize parent = 0; parent < context.asset.nodes.size(); ++parent) {
        for (u32 child : context.asset.nodes[parent].children) {
            if (context.asset.nodes[child].parent != -1) return context.Fail("glTF node has multiple parents");
            context.asset.nodes[child].parent = static_cast<i32>(parent);
        }
    }
    return true;
}

bool ReadScene(Context& context)
{
    return ReadNodes(context) && ReadSkins(context) && ReadAnimations(context);
}

} // namespace Concord::AssetGltf
