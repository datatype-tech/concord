// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDebugOverlay.h"

#include "engine/render/vulkan/VulkanDebugOverlayInternal.h"

#include <cstring>
#include <span>

namespace Concord {
namespace {

/** Largest number of glyphs the per-slot vertex buffer can hold. */
constexpr u32 kMaxOverlayGlyphs =
    static_cast<u32>(128 * 1024 / (6 * sizeof(OverlayVertex)));
/**
 * Half-texel inset applied to both the UV range and the quad geometry, so
 * pixel centers land exactly on texel centers. Insetting the UVs alone
 * would compress h texels into h pixels and starve the glyph's first and
 * last rows of coverage, clipping the tops of the letters.
 */
constexpr f32 kTexelInset = 0.5f;

/** Converts a device-pixel abscissa to Vulkan's y-down NDC. */
f32 PixelToNdcX(f32 pixel, f32 width) noexcept
{
    return width > 0.0f ? (pixel / width) * 2.0f - 1.0f : 0.0f;
}

/** Converts a device-pixel ordinate (top-left origin) to NDC. */
f32 PixelToNdcY(f32 pixel, f32 height) noexcept
{
    return height > 0.0f ? (pixel / height) * 2.0f - 1.0f : 0.0f;
}

/** NDC distance covered by a horizontal run of `pixels` device pixels. */
f32 PixelDeltaToNdcX(f32 pixels, f32 width) noexcept
{
    return width > 0.0f ? (pixels / width) * 2.0f : 0.0f;
}

/** NDC distance covered by a vertical run of `pixels` device pixels. */
f32 PixelDeltaToNdcY(f32 pixels, f32 height) noexcept
{
    return height > 0.0f ? (pixels / height) * 2.0f : 0.0f;
}

/** Appends one glyph rectangle, returning false past the capacity. */
bool AppendGlyph(OverlayVertex* vertices, u32 capacity, u32& count, f32 left, f32 top,
                 const DebugFontGlyph& glyph, f32 atlasWidth, f32 atlasHeight, f32 width,
                 f32 height)
{
    if (count + 6 > capacity) {
        return false;
    }
    if (glyph.w == 0 || glyph.h == 0) {
        return true;
    }
    const f32 u0 = (static_cast<f32>(glyph.x) + kTexelInset) / atlasWidth;
    const f32 u1 = (static_cast<f32>(glyph.x + glyph.w) - kTexelInset) / atlasWidth;
    const f32 v0 = (static_cast<f32>(glyph.y) + kTexelInset) / atlasHeight;
    const f32 v1 = (static_cast<f32>(glyph.y + glyph.h) - kTexelInset) / atlasHeight;
    const f32 x0 = PixelToNdcX(left + kTexelInset, width);
    const f32 x1 = PixelToNdcX(left + static_cast<f32>(glyph.w) - kTexelInset, width);
    const f32 y0 = PixelToNdcY(top + kTexelInset, height);
    const f32 y1 = PixelToNdcY(top + static_cast<f32>(glyph.h) - kTexelInset, height);
    const OverlayVertex quad[6] = {
        {x0, y0, u0, v0}, {x1, y0, u1, v0}, {x0, y1, u0, v1},
        {x0, y1, u0, v1}, {x1, y0, u1, v0}, {x1, y1, u1, v1},
    };
    std::memcpy(vertices + count, quad, sizeof(quad));
    count += 6;
    return true;
}

} // namespace

void RecordVulkanDebugOverlay(VkCommandBuffer commandBuffer, VulkanDebugOverlay& overlay,
                              u32 frameSlot, VkExtent2D extent, VkImageView colorView,
                              const DebugOverlayFrame& frame)
{
    if (commandBuffer == VK_NULL_HANDLE || !overlay.IsReady() || colorView == VK_NULL_HANDLE ||
        !frame.visible || frame.lineCount == 0 || frameSlot >= kMaxFramesInFlight ||
        extent.width == 0 || extent.height == 0) {
        return;
    }
    VulkanBuffer& buffer = overlay.vertices[frameSlot];
    OverlayVertex* vertices = static_cast<OverlayVertex*>(buffer.mapped);
    if (vertices == nullptr) {
        return;
    }
    const f32 width = static_cast<f32>(extent.width);
    const f32 height = static_cast<f32>(extent.height);
    const f32 atlasWidth = static_cast<f32>(overlay.font.width);
    const f32 atlasHeight = static_cast<f32>(overlay.font.height);
    const f32 lineAdvance = overlay.font.lineAdvance;

    u32 count = 0;
    const u32 capacity = kMaxOverlayGlyphs;
    for (u32 line = 0; line < frame.lineCount && line < kDebugOverlayMaxLines; ++line) {
        const char* text = frame.lines[line].text;
        const f32 penLeft = width - kOverlayMargin - overlay.font.LineWidth(text);
        const f32 penTop =
            static_cast<f32>(kOverlayMargin) + static_cast<f32>(line) * lineAdvance;
        f32 penX = penLeft;
        for (const char* character = text; *character != '\0'; ++character) {
            const unsigned char code = static_cast<unsigned char>(*character);
            if (code < 0x20u || code > 0x7Fu) {
                continue;
            }
            const DebugFontGlyph& glyph = overlay.font.glyphs[code - 0x20u];
            const bool fits =
                AppendGlyph(vertices, capacity, count, penX + glyph.left,
                            penTop + overlay.font.ascent + glyph.top, glyph, atlasWidth,
                            atlasHeight, width, height);
            penX += glyph.advance;
            if (!fits) {
                break;
            }
        }
    }
    if (count == 0 || !UploadVulkanBuffer(buffer, std::span<const std::byte>(
                                                   reinterpret_cast<const std::byte*>(vertices),
                                                   count * sizeof(OverlayVertex)))) {
        return;
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &colorAttachment;

    VkViewport viewport{};
    viewport.width = width;
    viewport.height = height;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, extent};
    const VkDeviceSize offset = 0;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlay.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlay.layout, 0, 1,
                            &overlay.descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &buffer.buffer, &offset);

    OverlayPushConstants push{};
    push.color[0] = 0.0f;
    push.color[1] = 0.0f;
    push.color[2] = 0.0f;
    push.color[3] = 0.85f;
    push.offset[0] = PixelDeltaToNdcX(kOverlayShadowOffset, width);
    push.offset[1] = PixelDeltaToNdcY(kOverlayShadowOffset, height);

    vkCmdBeginRendering(commandBuffer, &rendering);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdPushConstants(commandBuffer, overlay.layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(push), &push);
    vkCmdDraw(commandBuffer, count, 1, 0, 0);
    push.color[0] = 1.0f;
    push.color[1] = 1.0f;
    push.color[2] = 1.0f;
    push.color[3] = 1.0f;
    push.offset[0] = 0.0f;
    push.offset[1] = 0.0f;
    vkCmdPushConstants(commandBuffer, overlay.layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(push), &push);
    vkCmdDraw(commandBuffer, count, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
