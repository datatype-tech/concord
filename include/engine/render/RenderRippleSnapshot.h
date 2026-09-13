// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERRIPPLESNAPSHOT_H
#define CONCORD_RENDERRIPPLESNAPSHOT_H

#include "engine/core/Types.h"
#include "engine/render/RenderSceneSnapshot.h"

#include <vector>

namespace Concord {

struct World;

/**
 * Copies every live water disturbance into the frame's ripple list.
 *
 * The list is bounded by `kMaxRenderRipples`; anything past the bound is
 * dropped rather than grown, because the shader loops over it per water pixel.
 */
void AppendRippleSnapshots(std::vector<RenderRippleSnapshot>& output,
                           const World& world) noexcept;

} // namespace Concord

#endif // CONCORD_RENDERRIPPLESNAPSHOT_H
