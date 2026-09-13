// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/asset/SkinnedGeometry.h"

#include <span>

namespace Concord {
namespace {

/** Finds the snapshot object carrying this entity's animated pose. */
const RenderObjectSnapshot* FindPosedObject(const RenderSceneSnapshot& snapshot,
                                            Entity entity) noexcept
{
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.entity == entity && object.skinningRange.jointCount != 0) {
            return &object;
        }
    }
    return nullptr;
}

/** Overwrites one source's packed vertices with its entity's deformed pose. */
bool DeformSource(VulkanRayTracingScene& scene, VulkanRayTracingSkinnedSource& source,
                  const RenderObjectSnapshot& object, std::span<const Mat4> joints)
{
    if (source.metadataFirstVertex > scene.modelVertices.size() ||
        source.vertexCount > scene.modelVertices.size() - source.metadataFirstVertex) {
        return false;
    }
    if (source.scratch.size() < source.vertexCount) {
        source.scratch.resize(source.vertexCount);
    }
    const std::span<SkinnedVertex> solved(source.scratch.data(), source.vertexCount);
    if (!SolveSkinnedVertices(source.rest, joints, object.skinningRange.firstJoint,
                              object.skinningRange.jointCount, solved)) {
        return false;
    }
    for (u32 index = 0; index < source.vertexCount; ++index) {
        const SkinnedVertex& deformed = solved[index];
        VulkanRayTracingModelVertex& vertex =
            scene.modelVertices[source.metadataFirstVertex + index];
        vertex.position = {deformed.position.x, deformed.position.y, deformed.position.z, 1.0f};
        vertex.normal = {deformed.normal.x, deformed.normal.y, deformed.normal.z, 0.0f};
        // The bound texcoord is left alone: skinning deforms positions and
        // normals only, and the hit shader still needs the authored UVs.
    }
    return true;
}

} // namespace

bool UpdateVulkanRayTracingSkinnedGeometry(VulkanRayTracingScene& scene,
                                           const RenderSceneSnapshot& snapshot)
{
    if (scene.skinnedSources.empty()) {
        return true;
    }
    if (!scene.modelVertexBuffer.IsReady()) {
        return false;
    }
    const std::span<const Mat4> joints(snapshot.skinningPalette.jointMatrices);
    bool deformed = false;
    for (VulkanRayTracingSkinnedSource& source : scene.skinnedSources) {
        const RenderObjectSnapshot* object = FindPosedObject(snapshot, source.entity);
        if (object == nullptr) {
            continue;
        }
        if (!DeformSource(scene, source, *object, joints)) {
            return false;
        }
        deformed = true;
    }
    if (!deformed) {
        return true;
    }
    const std::span<const VulkanRayTracingModelVertex> packed(scene.modelVertices);
    return UploadVulkanBuffer(scene.modelVertexBuffer, std::as_bytes(packed));
}

} // namespace Concord
