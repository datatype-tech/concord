// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_MODELRENDERER_H
#define CONCORD_MODELRENDERER_H

#include "engine/asset/ModelAsset.h"

#include <memory>

namespace Concord {

inline constexpr u32 kAllModelMeshes = 0xFFFFFFFFu;

/** References decoded model data for one scene entity. */
struct ModelRenderer {
    /** Shared immutable CPU asset retained by render snapshots. */
    std::shared_ptr<const ModelAsset> asset{};
    /** Mesh to draw, or kAllModelMeshes for every mesh in the asset. */
    u32 meshIndex = kAllModelMeshes;
    /** Whether the instance participates in visible rendering. */
    bool visible = true;
    /** Whether the instance contributes to shadow and RT passes. */
    bool castShadow = true;
};

} // namespace Concord

#endif // CONCORD_MODELRENDERER_H
