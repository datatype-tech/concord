// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONSYSTEM_H
#define CONCORD_ANIMATIONSYSTEM_H

#include "Concord/CExport.h"
#include "engine/ecs/System.h"
#include "engine/ecs/World.h"

namespace Concord {

/** Advances AnimationComponent values and refreshes their cached skin poses. */
class CENGINE_API AnimationSystem final : public ISystem {
public:
    /** Samples every valid animation component in the scene. */
    void OnUpdate(Scene& scene, f32 deltaTime) override;
};

/** Updates animation components without requiring a scheduled system. */
CENGINE_API usize UpdateAnimationComponents(World& world, f32 deltaTime) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONSYSTEM_H
