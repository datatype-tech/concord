// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERSURFACEQUERY_H
#define CONCORD_WATERSURFACEQUERY_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

#include <vector>

namespace Concord {

class World;

/**
 * One water surface's world-space footprint, snapshotted for a frame.
 *
 * Water carries no Jolt collider (see `WaterBodyComponent`), so every system
 * that needs to know where the water is has to derive it from the components
 * rather than ask the solver. Shared rather than re-derived per system: two
 * systems disagreeing about where the waterline sits is the kind of defect
 * that shows up as a splash at one height and a float at another.
 */
struct WaterSurface {
    f32 worldY = 0.0f;
    f32 centreX = 0.0f;
    f32 centreZ = 0.0f;
    f32 halfExtentX = 0.0f;
    f32 halfExtentZ = 0.0f;
};

/**
 * Rebuilds `surfaces` from every `WaterBodyComponent` in the world.
 *
 * Surfaces with either half extent at zero are skipped: that is the sentinel a
 * vertical waterfall sheet is spawned with, and it has no horizontal footprint
 * for anything to fall onto or float in.
 */
CENGINE_API void CollectWaterSurfaces(World& world, std::vector<WaterSurface>& surfaces);

/**
 * The surface covering a horizontal position, or nullptr when none does.
 *
 * The first match wins rather than the highest. Overlapping bodies of water are
 * an authoring mistake -- two water materials in the same cells z-fight when
 * drawn -- so resolving the overlap sensibly here would only hide it.
 */
[[nodiscard]] CENGINE_API const WaterSurface* FindWaterSurface(
    const std::vector<WaterSurface>& surfaces, f32 x, f32 z) noexcept;

} // namespace Concord

#endif // CONCORD_WATERSURFACEQUERY_H
