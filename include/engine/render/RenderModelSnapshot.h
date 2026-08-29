// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERMODELSNAPSHOT_H
#define CONCORD_RENDERMODELSNAPSHOT_H

#include "engine/core/Mat4.h"
#include "engine/ecs/Entity.h"
#include "engine/render/RenderSceneSnapshot.h"
#include "engine/scene/ModelRenderer.h"

namespace Concord {

/** Expands an imported model instance into node-aware render objects. */
void AppendModelSnapshots(RenderSceneSnapshot& snapshot, Entity entity,
                          const ModelRenderer& model, Mat4 instanceModel);

} // namespace Concord

#endif // CONCORD_RENDERMODELSNAPSHOT_H
