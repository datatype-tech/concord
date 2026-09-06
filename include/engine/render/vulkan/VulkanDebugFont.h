// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANDEBUGFONT_H
#define CONCORD_VULKANDEBUGFONT_H

#include "engine/core/Types.h"

#include <vector>

namespace Concord {

/** Printable ASCII glyph count baked into a debug font atlas. */
inline constexpr u32 kDebugFontCodepoints = 96;

/** One glyph's placement: atlas rectangle and pen-relative drawing offset. */
struct DebugFontGlyph {
    /** Horizontal pen advance in pixels at the baked size. */
    f32 advance = 0.0f;
    /** Pen-relative offset to the rectangle's top-left corner; y is down
     *  from the line's ascent. */
    f32 left = 0.0f;
    f32 top = 0.0f;
    /** Atlas rectangle in texels. */
    u16 x = 0;
    u16 y = 0;
    u16 w = 0;
    u16 h = 0;
};

/**
 * A baked single-channel font atlas for the debug text overlay.
 *
 * `BakeDebugFont` prefers a Windows system TrueType font (anti-aliased,
 * variable-width) and falls back to the bundled 8x8 bitmap glyphs scaled to
 * the requested size, so the bake always succeeds on Windows.
 */
struct DebugFontBake {
    DebugFontGlyph glyphs[kDebugFontCodepoints]{};
    /** R8 texels, row-major, `width * height` bytes; empty when invalid. */
    std::vector<unsigned char> texels;
    u32 width = 0;
    u32 height = 0;
    /** Distance from a line's top to its baseline, in pixels. */
    f32 ascent = 0.0f;
    /** Distance from one line's top to the next, in pixels. */
    f32 lineAdvance = 0.0f;
    /** True for anti-aliased bakes, which sample with bilinear filtering. */
    bool smooth = false;
    /** True when texels and metrics were produced by a successful bake. */
    bool loaded = false;

    /** Horizontal pixel width of a NUL-terminated ASCII line. */
    [[nodiscard]] f32 LineWidth(const char* text) const noexcept;
};

/**
 * Bakes printable ASCII (0x20..0x7F) at the requested pixel height.
 *
 * @param pixelHeight Target cap height-ish size; the bake is one-to-one
 *        pixels, so the overlay draws at exactly this size.
 */
[[nodiscard]] DebugFontBake BakeDebugFont(f32 pixelHeight);

} // namespace Concord

#endif // CONCORD_VULKANDEBUGFONT_H
