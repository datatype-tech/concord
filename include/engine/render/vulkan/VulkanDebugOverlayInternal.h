// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANDEBUGOVERLAYINTERNAL_H
#define CONCORD_VULKANDEBUGOVERLAYINTERNAL_H

#include "engine/core/Types.h"

namespace Concord {

/** Gap between text lines, in device pixels. */
constexpr u32 kOverlayLineSpacing = 6;
/** Distance from the window edges, in device pixels. */
constexpr u32 kOverlayMargin = 10;
/** Drop shadow distance behind the white text, in device pixels. */
constexpr f32 kOverlayShadowOffset = 2.0f;
/** Baked size of the overlay font, in pixels. */
constexpr f32 kOverlayFontPixelHeight = 20.0f;

/** Host-side vertex fed to the overlay pipeline; positions are NDC. */
struct OverlayVertex {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 u = 0.0f;
    f32 v = 0.0f;
};

/** Push-constant block shared by the overlay's vertex and fragment stages. */
struct OverlayPushConstants {
    f32 color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    f32 offset[2] = {0.0f, 0.0f};
    f32 reserved[2] = {0.0f, 0.0f};
};

static_assert(sizeof(OverlayPushConstants) == 32, "push-constant layout is shader-coupled");

} // namespace Concord

#endif // CONCORD_VULKANDEBUGOVERLAYINTERNAL_H
