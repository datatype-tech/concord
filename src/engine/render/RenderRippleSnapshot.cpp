// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderRippleSnapshot.h"

#include "engine/ecs/WaterRippleComponents.h"
#include "engine/ecs/World.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Keeps a malformed authored value from reaching the shader as a NaN slope. */
f32 Safe(f32 value, f32 fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

void AppendRippleSnapshots(std::vector<RenderRippleSnapshot>& output, const World& world) noexcept
{
    try {
        world.Query<WaterRippleComponent>(
            [&output](Entity, const WaterRippleComponent& component) {
                if (output.size() >= kMaxRenderRipples) {
                    return;
                }
                const WaterRipple& ring = component.ripple;
                const f32 lifetime = Safe(component.lifetime, 0.0f);
                if (lifetime <= 0.0f) {
                    return;
                }
                // Faded here rather than in the shader: the ring is drawn at the
                // amplitude it should have, and nothing downstream has to know
                // that anything is dying. The curve starts at full strength so a
                // fresh splash is not already half gone, and reaches exactly zero
                // so the last frames are not a flicker.
                const f32 progress =
                    std::clamp(Safe(component.age, 0.0f) / lifetime, 0.0f, 1.0f);
                const f32 fade = 1.0f - progress * progress;
                const f32 strength = Safe(ring.strength, 0.0f) * fade;
                if (strength <= 0.0f) {
                    return;
                }
                output.push_back(RenderRippleSnapshot{
                    .centre = {Safe(ring.centre.x, 0.0f), Safe(ring.centre.y, 0.0f)},
                    .wavelength = Safe(ring.wavelength, 3.0f),
                    .speed = Safe(ring.speed, 0.0f),
                    .strength = strength,
                    .falloff = std::max(Safe(ring.falloff, 0.0f), 0.0f),
                    .reach = Safe(ring.reach, 24.0f),
                    .age = std::max(Safe(component.age, 0.0f), 0.0f),
                });
            });
    } catch (...) {
        // A snapshot is best-effort: a scene that cannot be walked still has to
        // produce a frame rather than take the renderer down with it.
        output.clear();
    }
}

} // namespace Concord
