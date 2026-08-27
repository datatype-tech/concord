// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_COLOR_H
#define CONCORD_COLOR_H

#include "engine/core/Types.h"
#include "engine/core/Vec3.h"

namespace Concord {

/** Packed 32-bit color, laid out as 0xAARRGGBB. */
using ColorRGBA = u32;

/** Builds a packed color from 8-bit components. */
[[nodiscard]] constexpr ColorRGBA MakeColor(u8 r, u8 g, u8 b, u8 a = 255) noexcept
{
    return (static_cast<u32>(a) << 24) | (static_cast<u32>(r) << 16) |
           (static_cast<u32>(g) << 8) | static_cast<u32>(b);
}

[[nodiscard]] constexpr u8 ColorR(ColorRGBA c) noexcept { return static_cast<u8>((c >> 16) & 0xFFu); }
[[nodiscard]] constexpr u8 ColorG(ColorRGBA c) noexcept { return static_cast<u8>((c >> 8) & 0xFFu); }
[[nodiscard]] constexpr u8 ColorB(ColorRGBA c) noexcept { return static_cast<u8>(c & 0xFFu); }
[[nodiscard]] constexpr u8 ColorA(ColorRGBA c) noexcept { return static_cast<u8>((c >> 24) & 0xFFu); }

/**
 * Decodes a packed sRGB color to linear space for shading.
 *
 * Uses the gamma 2.2 approximation rather than the exact piecewise sRGB
 * curve: the difference is imperceptible for authored colors and it keeps
 * this usable in constant-folded paths.
 */
[[nodiscard]] inline Vec3 ToLinear(ColorRGBA c) noexcept
{
    constexpr f32 inv = 1.0f / 255.0f;
    const f32 r = static_cast<f32>(ColorR(c)) * inv;
    const f32 g = static_cast<f32>(ColorG(c)) * inv;
    const f32 b = static_cast<f32>(ColorB(c)) * inv;
    return {r * r, g * g, b * b};
}

} // namespace Concord

/** Shorthand application code writes inside a Desc: `COLOR_RGB(224, 64, 64)`. */
#define COLOR_RGB(r, g, b) ::Concord::MakeColor((r), (g), (b))

/** Shorthand with an explicit alpha component. */
#define COLOR_RGBA(r, g, b, a) ::Concord::MakeColor((r), (g), (b), (a))

#endif // CONCORD_COLOR_H
