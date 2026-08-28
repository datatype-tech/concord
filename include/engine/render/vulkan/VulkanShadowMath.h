// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADOWMATH_H
#define CONCORD_VULKANSHADOWMATH_H

#include "engine/core/Mat4.h"
#include "engine/core/Types.h"
#include "engine/render/RenderSceneSnapshot.h"

namespace Concord {

/** CPU-side selection and transform for the optional sun shadow pass. */
struct VulkanDirectionalShadowState {
    bool enabled = false;
    usize lightIndex = 0;
    Mat4 viewProjection = Mat4::Identity();
};

/** Selects the first shadow-casting directional light and frames the scene. */
[[nodiscard]] VulkanDirectionalShadowState BuildVulkanDirectionalShadowState(
    const RenderSceneSnapshot& snapshot) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANSHADOWMATH_H
