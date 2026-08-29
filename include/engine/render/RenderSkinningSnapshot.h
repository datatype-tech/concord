// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERSKINNINGSNAPSHOT_H
#define CONCORD_RENDERSKINNINGSNAPSHOT_H

#include "engine/ecs/World.h"
#include "engine/render/RenderSceneSnapshot.h"

namespace Concord {

/** Appends valid entity poses to a frame snapshot and tags matching draws. */
void AppendSkinningSnapshots(RenderSceneSnapshot& snapshot, const World& world);

} // namespace Concord

#endif // CONCORD_RENDERSKINNINGSNAPSHOT_H
