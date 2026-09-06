// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDebugFont.h"

#include "engine/render/vulkan/DebugFont8x8.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Concord {
namespace {

/** 1 texel of padding between baked glyphs stops linear filter bleeding. */
constexpr u32 kGlyphPadding = 1;

/** Loads a Windows system font likely to exist on any desktop install. */
std::vector<unsigned char> ReadSystemFont()
{
    const char* candidates[] = {
        "C:/Windows/Fonts/consola.ttf", // Consolas: monospace, ideal for stats
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
    };
    for (const char* path : candidates) {
        std::FILE* file = nullptr;
#if defined(_MSC_VER)
        fopen_s(&file, path, "rb");
#else
        file = std::fopen(path, "rb");
#endif
        if (file == nullptr) {
            continue;
        }
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        std::vector<unsigned char> bytes(size > 0 ? static_cast<std::size_t>(size) : 0u);
        const std::size_t read = bytes.empty() ? 0u : std::fread(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
        if (read == bytes.size() && !bytes.empty()) {
            return bytes;
        }
    }
    return {};
}

/** Places one baked glyph into the atlas, advancing the row cursor. */
void PlaceGlyph(DebugFontBake& bake, DebugFontGlyph& entry, u32& cursorX, u32& cursorY,
                u32& rowHeight, const unsigned char* bitmap, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (cursorX + static_cast<u32>(w) + kGlyphPadding > bake.width) {
        cursorX = kGlyphPadding;
        cursorY += rowHeight + kGlyphPadding;
        rowHeight = 0;
    }
    for (int row = 0; row < h; ++row) {
        unsigned char* dest = bake.texels.data() +
                              (static_cast<std::size_t>(cursorY + row) * bake.width) +
                              cursorX;
        std::memcpy(dest, bitmap + static_cast<std::size_t>(row) * w, static_cast<std::size_t>(w));
    }
    // Record the rectangle only after placement: the row cursor may have
    // wrapped, and the entry must match where the texels actually landed.
    entry.x = static_cast<u16>(cursorX);
    entry.y = static_cast<u16>(cursorY);
    entry.w = static_cast<u16>(w);
    entry.h = static_cast<u16>(h);
    rowHeight = std::max(rowHeight, static_cast<u32>(h));
    cursorX += static_cast<u32>(w) + kGlyphPadding;
}

/**
 * Bakes the bundled 8x8 bitmap glyphs to `pixelHeight` by integer block
 * expansion; used only when no system TrueType font can be read.
 */
DebugFontBake BakeBitmapFont(f32 pixelHeight)
{
    const u32 scale = std::max<u32>(1u, static_cast<u32>(std::lround(pixelHeight / 8.0f)));
    DebugFontBake bake;
    bake.width = kDebugFontGlyphCount * 8 * scale;
    bake.height = 8 * scale;
    bake.texels.assign(static_cast<std::size_t>(bake.width) * bake.height, 0);
    for (u32 glyph = 0; glyph < kDebugFontGlyphCount; ++glyph) {
        DebugFontGlyph& entry = bake.glyphs[glyph];
        entry.advance = static_cast<f32>(8 * scale);
        entry.x = static_cast<u16>(glyph * 8 * scale);
        entry.w = static_cast<u16>(8 * scale);
        entry.h = static_cast<u16>(8 * scale);
        for (u32 row = 0; row < 8; ++row) {
            const unsigned char bits = kDebugFont8x8[glyph][row];
            for (u32 column = 0; column < 8; ++column) {
                if (((bits >> column) & 1u) == 0u) {
                    continue;
                }
                for (u32 y = 0; y < scale; ++y) {
                    unsigned char* dest = bake.texels.data() +
                                          (static_cast<std::size_t>(row * scale + y) * bake.width) +
                                          glyph * 8 * scale + column * scale;
                    std::memset(dest, 255, scale);
                }
            }
        }
    }
    bake.ascent = static_cast<f32>(8 * scale);
    bake.lineAdvance = static_cast<f32>(8 * scale + 4);
    bake.smooth = false;
    bake.loaded = true;
    return bake;
}

} // namespace

f32 DebugFontBake::LineWidth(const char* text) const noexcept
{
    f32 width = 0.0f;
    for (const char* character = text; *character != '\0'; ++character) {
        const unsigned char code = static_cast<unsigned char>(*character);
        if (code >= 0x20u && code <= 0x7Fu) {
            width += glyphs[code - 0x20u].advance;
        }
    }
    return width;
}

DebugFontBake BakeDebugFont(f32 pixelHeight)
{
    const std::vector<unsigned char> fontBytes = ReadSystemFont();
    if (fontBytes.empty()) {
        return BakeBitmapFont(pixelHeight);
    }

    stbtt_fontinfo font{};
    const int offset = stbtt_GetFontOffsetForIndex(fontBytes.data(), 0);
    if (offset < 0 || stbtt_InitFont(&font, fontBytes.data(), offset) == 0) {
        return BakeBitmapFont(pixelHeight);
    }

    const f32 scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
    int metricsAscent = 0;
    int metricsDescent = 0;
    int metricsLineGap = 0;
    stbtt_GetFontVMetrics(&font, &metricsAscent, &metricsDescent, &metricsLineGap);

    // Conservative upper bound; the row packer compacts it afterwards.
    DebugFontBake bake;
    bake.width = 2048;
    bake.texels.assign(static_cast<std::size_t>(bake.width) * 256, 0);

    bake.ascent = static_cast<f32>(metricsAscent) * scale;
    bake.lineAdvance =
        static_cast<f32>(metricsAscent - metricsDescent + metricsLineGap) * scale + 4.0f;

    u32 cursorX = kGlyphPadding;
    u32 cursorY = kGlyphPadding;
    u32 rowHeight = 0;
    std::vector<unsigned char> bitmap;
    for (u32 index = 0; index < kDebugFontCodepoints; ++index) {
        const int codepoint = 0x20 + static_cast<int>(index);
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
        const int w = x1 - x0;
        const int h = y1 - y0;
        DebugFontGlyph& entry = bake.glyphs[index];
        int advance = 0;
        int leftBearing = 0;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &leftBearing);
        entry.advance = static_cast<f32>(advance) * scale;
        entry.left = static_cast<f32>(x0);
        entry.top = static_cast<f32>(y0);
        if (w <= 0 || h <= 0) {
            continue;
        }
        bitmap.resize(static_cast<std::size_t>(w) * h);
        stbtt_MakeCodepointBitmap(&font, bitmap.data(), w, h, w, scale, scale, codepoint);
        PlaceGlyph(bake, entry, cursorX, cursorY, rowHeight, bitmap.data(), w, h);
    }
    if (cursorX == kGlyphPadding && cursorY == kGlyphPadding) {
        return BakeBitmapFont(pixelHeight);
    }
    bake.height = cursorY + rowHeight + kGlyphPadding;
    bake.texels.resize(static_cast<std::size_t>(bake.width) * bake.height);
    bake.smooth = true;
    bake.loaded = true;
    return bake;
}

} // namespace Concord
