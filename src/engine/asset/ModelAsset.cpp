// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ModelAsset.h"

#include <cmath>

namespace Concord {

namespace {

bool Finite(Vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Finite(Quat value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

bool Finite(const BoneTransform& transform) noexcept
{
    return Finite(transform.translation) && Finite(transform.scale) &&
           Finite(transform.rotation);
}

} // namespace

bool ModelAsset::IsValid() const noexcept
{
    if (meshes.empty() || materials.empty()) return false;
    for (const ModelMaterial& material : materials) {
        if (!std::isfinite(material.metallic) || !std::isfinite(material.roughness) ||
            material.metallic < 0.0f || material.metallic > 1.0f || material.roughness <= 0.0f ||
            material.roughness > 1.0f || !Finite(material.emissive)) return false;
    }
    for (const ModelMesh& mesh : meshes) {
        if (mesh.primitives.empty()) return false;
        for (const ModelPrimitive& primitive : mesh.primitives) {
            if (primitive.vertices.empty() || primitive.indices.size() < 3 ||
                primitive.indices.size() % 3 != 0 || primitive.mode != 4 || primitive.materialIndex >= materials.size()) {
                return false;
            }
            for (const ModelVertex& vertex : primitive.vertices) {
                if (!Finite(vertex.position) || !Finite(vertex.normal) ||
                    !Finite(Vec3{vertex.tangent.x, vertex.tangent.y, vertex.tangent.z}) ||
                    !std::isfinite(vertex.tangent.w) || !std::isfinite(vertex.texcoord.x) ||
                    !std::isfinite(vertex.texcoord.y) || !Finite(Vec3{vertex.weights.x, vertex.weights.y, vertex.weights.z}) ||
                    !std::isfinite(vertex.weights.w)) return false;
            }
            for (u32 index : primitive.indices) {
                if (index >= primitive.vertices.size()) return false;
            }
        }
    }
    for (const Skeleton& skeleton : skeletons) {
        if (!skeleton.IsValid() || (!skeleton.nodeIndices.empty() && skeleton.nodeIndices.size() != skeleton.joints.size())) return false;
    }
    for (const ModelNode& node : nodes) {
        if (!Finite(node.local) || node.parent < -1 ||
            (node.parent >= 0 && static_cast<usize>(node.parent) >= nodes.size()) ||
            (node.mesh >= 0 && static_cast<usize>(node.mesh) >= meshes.size()) ||
            (node.skin >= 0 && static_cast<usize>(node.skin) >= skeletons.size())) return false;
        for (u32 child : node.children) if (child >= nodes.size()) return false;
    }
    for (usize index = 0; index < nodes.size(); ++index) {
        std::vector<u8> seen(nodes.size(), 0);
        i32 current = static_cast<i32>(index);
        while (current >= 0) {
            const usize node = static_cast<usize>(current);
            if (seen[node] != 0) return false;
            seen[node] = 1;
            current = nodes[node].parent;
        }
    }
    for (const AnimationClip& clip : animations) {
        if (!std::isfinite(clip.duration) || clip.duration < 0.0f) return false;
        for (const AnimationChannel& channel : clip.channels) {
            if (channel.sourceNode != kInvalidJoint) {
                if (channel.sourceNode >= nodes.size()) return false;
            } else {
                bool validJoint = false;
                for (const Skeleton& skeleton : skeletons) {
                    if (channel.joint < skeleton.joints.size()) { validJoint = true; break; }
                }
                if (!validJoint) return false;
            }
            if (channel.path == AnimationPath::Rotation) {
                for (usize i = 0; i < channel.rotationKeys.size(); ++i) {
                    const auto& key = channel.rotationKeys[i];
                    if (!std::isfinite(key.time) || !Finite(key.value) || !Finite(key.inTangent) || !Finite(key.outTangent)) return false;
                    if (i && !(channel.rotationKeys[i - 1].time <= key.time)) return false;
                }
            } else {
                for (usize i = 0; i < channel.vec3Keys.size(); ++i) {
                    const auto& key = channel.vec3Keys[i];
                    if (!std::isfinite(key.time) || !Finite(key.value) || !Finite(key.inTangent) || !Finite(key.outTangent)) return false;
                    if (i && !(channel.vec3Keys[i - 1].time <= key.time)) return false;
                }
            }
        }
    }
    return true;
}

const AnimationClip* ModelAsset::FindAnimation(const std::string& clipName) const noexcept
{
    for (const AnimationClip& animation : animations) {
        if (animation.name == clipName) return &animation;
    }
    return nullptr;
}

} // namespace Concord
