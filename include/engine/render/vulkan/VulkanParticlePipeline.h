// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPARTICLEPIPELINE_H
#define CONCORD_VULKANPARTICLEPIPELINE_H

#include "engine/render/RenderParticleSnapshot.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanFrameLimits.h"

#include <vulkan/vulkan.h>

namespace Concord {

/**
 * Billboard pipelines and their per-frame vertex buffers.
 *
 * Two pipelines sharing one vertex stage: an additive one for emitters that
 * glow and a scattering one for emitters that occlude. They differ in blend
 * state and fragment shader and in nothing else, which is why the snapshot can
 * serve both from one buffer with a single split index.
 *
 * The pass owns no descriptor set of its own: it reuses the frame-data layout
 * for the camera matrices, exactly as the tile-culling compute pass does, so
 * adding particles costs no new binding surface.
 */
struct VulkanParticlePipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    /** Adds light; draws the vertices before `additiveVertices`. */
    VkPipeline pipeline = VK_NULL_HANDLE;
    /** Occludes and scatters; draws the vertices after it. */
    VkPipeline scatterPipeline = VK_NULL_HANDLE;
    VulkanBuffer vertexBuffers[kMaxFramesInFlight]{};

    /** Whether the pipeline objects exist, independent of buffer readiness. */
    [[nodiscard]] bool HasPipeline() const noexcept
    {
        return layout != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE &&
               scatterPipeline != VK_NULL_HANDLE;
    }

    /** Whether the pipeline and every per-frame vertex buffer are usable. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        if (!HasPipeline()) {
            return false;
        }
        for (const VulkanBuffer& buffer : vertexBuffers) {
            if (!buffer.IsReady()) {
                return false;
            }
        }
        return true;
    }
};

/** Creates the additive billboard pipeline and its per-frame vertex buffers. */
bool CreateVulkanParticlePipeline(const VulkanContext& context, VkFormat colorFormat,
                                  VkFormat depthFormat, VkDescriptorSetLayout frameDataLayout,
                                  VulkanParticlePipeline& output);

/** Releases the pipeline, its layout and every vertex buffer. */
void DestroyVulkanParticlePipeline(const VulkanContext& context,
                                   VulkanParticlePipeline& pipeline) noexcept;

/**
 * Uploads one frame of billboards and records both draw passes.
 *
 * The pass loads the existing colour and depth attachments rather than clearing
 * them, so it composes over whichever shading path produced the frame, and it
 * keeps depth testing while disabling depth writes.
 *
 * Additive emitters are drawn first and scattering ones second, which is the
 * order that matters: light adds on top of smoke rather than being swallowed by
 * it, and neither kind needs the particles sorted against each other.
 */
void RecordVulkanParticlePass(VkCommandBuffer commandBuffer, VulkanParticlePipeline& pipeline,
                              u32 frameIndex, const RenderParticleSnapshot& particles,
                              VkDescriptorSet frameDataSet, VkImageView colorView,
                              VkImageView depthView, VkExtent2D extent) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANPARTICLEPIPELINE_H
