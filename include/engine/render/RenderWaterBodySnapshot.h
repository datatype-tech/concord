// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERWATERBODYSNAPSHOT_H
#define CONCORD_RENDERWATERBODYSNAPSHOT_H

#include "engine/core/Types.h"
#include "engine/render/RenderSceneSnapshot.h"

#include <vector>

namespace Concord {

struct World;

/**
 * Copies every `WaterBodyComponent` into the frame's wetness list.
 *
 * The list is bounded by `kMaxWetnessBodies`; anything past the bound is
 * dropped rather than grown, because the shader tests every dry fragment
 * against it.
 */
void AppendWaterBodySnapshots(std::vector<RenderWaterBodySnapshot>& output,
                              const World& world) noexcept;

} // namespace Concord

#endif // CONCORD_RENDERWATERBODYSNAPSHOT_H
