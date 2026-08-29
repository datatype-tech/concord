// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <algorithm>
#include <cmath>

namespace Concord::AssetGltf {
namespace {

Mat4 MatrixAt(const std::vector<f32>& values, usize index) noexcept
{
    Mat4 matrix{};
    for (u32 column = 0; column < 4; ++column) {
        for (u32 row = 0; row < 4; ++row) matrix.col[column][row] = values[index * 16 + column * 4 + row];
    }
    return matrix;
}

bool Finite(const Mat4& matrix) noexcept
{
    for (const Vec4& column : matrix.col) {
        for (u32 component = 0; component < 4; ++component) {
            if (!std::isfinite(column[component])) return false;
        }
    }
    return true;
}

Mat4 MissingJointPath(const std::vector<ModelNode>& nodes, u32 nodeIndex,
                      const std::vector<i32>& nodeJoints) noexcept
{
    Mat4 result = Mat4::Identity();
    std::vector<u8> visited(nodes.size(), 0);
    i32 current = nodeIndex < nodes.size() ? nodes[nodeIndex].parent : -1;
    while (current >= 0 && static_cast<usize>(current) < nodes.size()) {
        const usize currentIndex = static_cast<usize>(current);
        if (visited[currentIndex] != 0) return Mat4::Identity();
        visited[currentIndex] = 1;
        if (nodeJoints[currentIndex] >= 0) break;
        result = nodes[currentIndex].local.ToMatrix() * result;
        current = nodes[currentIndex].parent;
    }
    return Finite(result) ? result : Mat4::Identity();
}

void FillExternalRoots(const std::vector<ModelNode>& nodes, Skeleton& skeleton)
{
    skeleton.externalRootTransforms.assign(skeleton.joints.size(), Mat4::Identity());
    std::vector<i32> nodeJoints(nodes.size(), -1);
    for (usize joint = 0; joint < skeleton.nodeIndices.size(); ++joint) {
        const u32 node = skeleton.nodeIndices[joint];
        if (node < nodes.size()) nodeJoints[node] = static_cast<i32>(joint);
    }
    for (usize joint = 0; joint < skeleton.joints.size(); ++joint) {
        if (joint >= skeleton.nodeIndices.size()) continue;
        skeleton.externalRootTransforms[joint] =
            MissingJointPath(nodes, skeleton.nodeIndices[joint], nodeJoints);
    }
}

void ResolveJointParents(const std::vector<ModelNode>& nodes, Skeleton& skeleton)
{
    std::vector<i32> nodeJoints(nodes.size(), -1);
    for (usize joint = 0; joint < skeleton.nodeIndices.size(); ++joint) {
        const u32 node = skeleton.nodeIndices[joint];
        if (node < nodes.size()) nodeJoints[node] = static_cast<i32>(joint);
    }
    for (usize joint = 0; joint < skeleton.nodeIndices.size(); ++joint) {
        const u32 node = skeleton.nodeIndices[joint];
        i32 parent = node < nodes.size() ? nodes[node].parent : -1;
        std::vector<u8> visited(nodes.size(), 0);
        while (parent >= 0 && static_cast<usize>(parent) < nodes.size() &&
               nodeJoints[static_cast<usize>(parent)] < 0) {
            if (visited[static_cast<usize>(parent)] != 0) {
                parent = -1;
                break;
            }
            visited[static_cast<usize>(parent)] = 1;
            parent = nodes[static_cast<usize>(parent)].parent;
        }
        skeleton.joints[joint].parent =
            parent >= 0 && static_cast<usize>(parent) < nodes.size()
                ? nodeJoints[static_cast<usize>(parent)]
                : -1;
    }
}

} // namespace

bool ReadSkins(Context& context)
{
    const AssetJson::Value* skins = Member(*context.root, "skins");
    context.asset.skeletons.clear();
    if (!skins) return true;
    if (!skins->Is(AssetJson::Type::Array)) return context.Fail("glTF skins must be an array");
    context.asset.skeletons.reserve(skins->array.size());
    for (const AssetJson::Value& record : skins->array) {
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF skin");
        const AssetJson::Value* joints = Member(record, "joints");
        if (!joints || !joints->Is(AssetJson::Type::Array) || joints->array.empty()) return context.Fail("glTF skin has no joints");
        Skeleton skeleton{};
        skeleton.name = std::string(Member(record, "name") ? Member(record, "name")->String() : std::string_view{});
        skeleton.joints.resize(joints->array.size());
        skeleton.nodeIndices.resize(joints->array.size());
        for (usize i = 0; i < joints->array.size(); ++i) {
            usize nodeIndex = 0;
            if (!IndexValue(&joints->array[i], nodeIndex) || nodeIndex >= context.asset.nodes.size()) return context.Fail("glTF skin joint node out of range");
            const ModelNode& node = context.asset.nodes[nodeIndex];
            skeleton.nodeIndices[i] = static_cast<u32>(nodeIndex);
            Joint& joint = skeleton.joints[i];
            joint.name = node.name;
            joint.local = node.local;
        }
        ResolveJointParents(context.asset.nodes, skeleton);
        const AssetJson::Value* inverse = Member(record, "inverseBindMatrices");
        if (inverse) {
            std::vector<f32> values;
            i32 inverseAccessor = -1;
            if (!SignedIndex(inverse, inverseAccessor) || !ReadFloatAccessor(context, inverseAccessor, 16, values) || values.size() != skeleton.joints.size() * 16) return context.Fail("invalid glTF inverse bind accessor");
            for (usize i = 0; i < skeleton.joints.size(); ++i) skeleton.joints[i].inverseBind = MatrixAt(values, i);
        }
        if (Member(record, "skeleton")) {
            usize rootNode = 0;
            if (!IndexValue(Member(record, "skeleton"), rootNode) || rootNode >= context.asset.nodes.size()) return context.Fail("invalid glTF skin root");
            for (usize i = 0; i < joints->array.size(); ++i) { usize nodeIndex = 0; IndexValue(&joints->array[i], nodeIndex); if (nodeIndex == rootNode) skeleton.root = static_cast<i32>(i); }
        }
        if (skeleton.root < 0) for (usize i = 0; i < skeleton.joints.size(); ++i) if (skeleton.joints[i].parent < 0) { skeleton.root = static_cast<i32>(i); break; }
        FillExternalRoots(context.asset.nodes, skeleton);
        if (!skeleton.IsValid()) return context.Fail("glTF skin hierarchy is invalid");
        context.asset.skeletons.push_back(std::move(skeleton));
    }
    for (ModelNode& node : context.asset.nodes) if (node.skin >= 0 && static_cast<usize>(node.skin) >= context.asset.skeletons.size()) return context.Fail("glTF node skin index out of range");
    return true;
}

} // namespace Concord::AssetGltf
