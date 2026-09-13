// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderWaterBodySnapshot.h"

#include "engine/core/Transform.h"
#include "engine/ecs/WaterBodyComponent.h"
#include "engine/ecs/World.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Keeps a malformed authored or derived value from reaching the shader as a NaN. */
f32 Safe(f32 value, f32 fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

void AppendWaterBodySnapshots(std::vector<RenderWaterBodySnapshot>& output,
                              const World& world) noexcept
{
    try {
        world.Query<WaterBodyComponent, Transform>(
            [&output](Entity, const WaterBodyComponent& body, const Transform& transform) {
                if (output.size() >= kMaxWetnessBodies) {
                    return;
                }
                const f32 halfX = body.extent.x * std::abs(Safe(transform.scale.x, 1.0f));
                const f32 halfZ = body.extent.y * std::abs(Safe(transform.scale.z, 1.0f));
                // Either axis at zero opts a surface out entirely -- the same
                // sentinel WaterSplashSystem reads a vertical waterfall sheet by.
                if (!(halfX > 0.0f) || !(halfZ > 0.0f)) {
                    return;
                }
                output.push_back(RenderWaterBodySnapshot{
                    .centre = {Safe(transform.position.x, 0.0f), Safe(transform.position.z, 0.0f)},
                    .worldY = Safe(transform.position.y, 0.0f),
                    .radius = std::max(halfX, halfZ),
                });
            });
    } catch (...) {
        // A snapshot is best-effort: a scene that cannot be walked still has to
        // produce a frame rather than take the renderer down with it.
        output.clear();
    }
}

} // namespace Concord
