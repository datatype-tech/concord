// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_MATERIAL_H
#define CONCORD_MATERIAL_H

#include "engine/core/Color.h"
#include "engine/core/Types.h"

namespace Concord {

/**
 * Surface appearance under a metallic-roughness workflow.
 *
 * Flattened to a single level on purpose: the first generation nested this
 * inside a `surface` sub-struct, which forced every call site to write
 * `.material = {.surface = {.albedo = ...}}`.
 */
struct Material {
    /** Base color. */
    ColorRGBA albedo = COLOR_RGB(200, 200, 200);

    /** Metalness, 0 for dielectrics and 1 for raw metal. */
    f32 metallic = 0.0f;

    /** Roughness, 0 for a mirror finish and 1 for fully diffuse. */
    f32 roughness = 0.8f;

    /** Self-illumination strength; 0 means the surface does not emit. */
    f32 emissive = 0.0f;
};

} // namespace Concord

#endif // CONCORD_MATERIAL_H
