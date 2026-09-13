// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDebugOverlay.h"

#include "engine/render/vulkan/VulkanDebugOverlayInternal.h"

#include <cstring>
#include <span>

namespace Concord {
namespace {

constexpr u32 kMaxOverlayGlyphs =
    static_cast<u32>(128 * 1024 / (6 * sizeof(OverlayVertex)));
constexpr f32 kTexelInset = 0.5f;
constexpr u32 kMaxOverlayDraws = 64;

struct OverlayDraw {
    u32 first = 0;
    u32 count = 0;
    f32 color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

f32 PixelToNdcX(f32 pixel, f32 width) noexcept
{
    return width > 0.0f ? (pixel / width) * 2.0f - 1.0f : 0.0f;
}

f32 PixelToNdcY(f32 pixel, f32 height) noexcept
{
    return height > 0.0f ? (pixel / height) * 2.0f - 1.0f : 0.0f;
}

bool AppendQuad(OverlayVertex* vertices, u32 capacity, u32& count, f32 left, f32 top,
                f32 right, f32 bottom, f32 u0, f32 v0, f32 u1, f32 v1, f32 width,
                f32 height)
{
    if (count + 6 > capacity) {
        return false;
    }
    const f32 x0 = PixelToNdcX(left, width);
    const f32 x1 = PixelToNdcX(right, width);
    const f32 y0 = PixelToNdcY(top, height);
    const f32 y1 = PixelToNdcY(bottom, height);
    const OverlayVertex quad[6] = {
        {x0, y0, u0, v0}, {x1, y0, u1, v0}, {x0, y1, u0, v1},
        {x0, y1, u0, v1}, {x1, y0, u1, v0}, {x1, y1, u1, v1},
    };
    std::memcpy(vertices + count, quad, sizeof(quad));
    count += 6;
    return true;
}

bool AppendGlyph(OverlayVertex* vertices, u32 capacity, u32& count, f32 left, f32 top,
                 const DebugFontGlyph& glyph, f32 atlasWidth, f32 atlasHeight, f32 width,
                 f32 height)
{
    if (glyph.w == 0 || glyph.h == 0) {
        return true;
    }
    const f32 u0 = (static_cast<f32>(glyph.x) + kTexelInset) / atlasWidth;
    const f32 u1 = (static_cast<f32>(glyph.x + glyph.w) - kTexelInset) / atlasWidth;
    const f32 v0 = (static_cast<f32>(glyph.y) + kTexelInset) / atlasHeight;
    const f32 v1 = (static_cast<f32>(glyph.y + glyph.h) - kTexelInset) / atlasHeight;
    return AppendQuad(vertices, capacity, count, left, top, left + static_cast<f32>(glyph.w),
                      top + static_cast<f32>(glyph.h), u0, v0, u1, v1, width, height);
}

bool AppendText(OverlayVertex* vertices, u32 capacity, u32& count, f32 penX, f32 penTop,
                const char* text, const DebugFontBake& font, f32 atlasWidth, f32 atlasHeight,
                f32 width, f32 height)
{
    if (text == nullptr) {
        return true;
    }
    for (const char* character = text; *character != '\0'; ++character) {
        const unsigned char code = static_cast<unsigned char>(*character);
        if (code < 0x20u || code > 0x7Fu) {
            continue;
        }
        const DebugFontGlyph& glyph = font.glyphs[code - 0x20u];
        if (!AppendGlyph(vertices, capacity, count, penX + glyph.left,
                         penTop + font.ascent + glyph.top, glyph, atlasWidth, atlasHeight,
                         width, height)) {
            return false;
        }
        penX += glyph.advance;
    }
    return true;
}

bool PushDraw(OverlayDraw* draws, u32& drawCount, u32 first, u32 count, const f32* color)
{
    if (count == 0 || drawCount >= kMaxOverlayDraws) {
        return false;
    }
    OverlayDraw& draw = draws[drawCount++];
    draw.first = first;
    draw.count = count;
    draw.color[0] = color[0];
    draw.color[1] = color[1];
    draw.color[2] = color[2];
    draw.color[3] = color[3];
    return true;
}

} // namespace

void RecordVulkanDebugOverlay(VkCommandBuffer commandBuffer, VulkanDebugOverlay& overlay,
                              u32 frameSlot, VkExtent2D extent, VkImageView colorView,
                              const DebugOverlayFrame* frame, const UiDrawList* ui)
{
    const bool hasOverlay = frame != nullptr && frame->visible && frame->lineCount > 0;
    const bool hasUi = ui != nullptr && !ui->commands.empty();
    if (commandBuffer == VK_NULL_HANDLE || !overlay.IsReady() || colorView == VK_NULL_HANDLE ||
        frameSlot >= kMaxFramesInFlight || extent.width == 0 || extent.height == 0 ||
        (!hasOverlay && !hasUi)) {
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
    OverlayDraw draws[kMaxOverlayDraws]{};
    u32 drawCount = 0;

    if (hasUi && overlay.font.hasWhite && atlasWidth > 0.0f && atlasHeight > 0.0f) {
        const f32 u0 = (static_cast<f32>(overlay.font.whiteX) + 0.5f) / atlasWidth;
        const f32 v0 = (static_cast<f32>(overlay.font.whiteY) + 0.5f) / atlasHeight;
        const f32 u1 = (static_cast<f32>(overlay.font.whiteX + overlay.font.whiteW) - 0.5f) /
                       atlasWidth;
        const f32 v1 = (static_cast<f32>(overlay.font.whiteY + overlay.font.whiteH) - 0.5f) /
                       atlasHeight;
        for (const UiDrawCommand& command : ui->commands) {
            const u32 start = count;
            if (command.kind == UiDrawKind::Rect) {
                if (!AppendQuad(vertices, capacity, count, command.x, command.y,
                                command.x + command.width, command.y + command.height, u0, v0,
                                u1, v1, width, height)) {
                    break;
                }
            } else if (command.kind == UiDrawKind::Text) {
                if (!AppendText(vertices, capacity, count, command.x, command.y, command.text,
                                overlay.font, atlasWidth, atlasHeight, width, height)) {
                    break;
                }
            }
            PushDraw(draws, drawCount, start, count - start, command.color.data());
        }
    }

    if (hasOverlay) {
        const u32 start = count;
        const f32 white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        for (u32 line = 0; line < frame->lineCount && line < kDebugOverlayMaxLines; ++line) {
            const char* text = frame->lines[line].text;
            const f32 penLeft = width - kOverlayMargin - overlay.font.LineWidth(text);
            const f32 penTop =
                static_cast<f32>(kOverlayMargin) + static_cast<f32>(line) * lineAdvance;
            if (!AppendText(vertices, capacity, count, penLeft, penTop, text, overlay.font,
                            atlasWidth, atlasHeight, width, height)) {
                break;
            }
        }
        PushDraw(draws, drawCount, start, count - start, white);
    }

    if (count == 0 || drawCount == 0 ||
        !UploadVulkanBuffer(buffer, std::span<const std::byte>(
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

    vkCmdBeginRendering(commandBuffer, &rendering);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    for (u32 index = 0; index < drawCount; ++index) {
        OverlayPushConstants push{};
        push.color[0] = draws[index].color[0];
        push.color[1] = draws[index].color[1];
        push.color[2] = draws[index].color[2];
        push.color[3] = draws[index].color[3];
        vkCmdPushConstants(commandBuffer, overlay.layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push), &push);
        vkCmdDraw(commandBuffer, draws[index].count, 1, draws[index].first, 0);
    }
    vkCmdEndRendering(commandBuffer);
}

} // namespace Concord
