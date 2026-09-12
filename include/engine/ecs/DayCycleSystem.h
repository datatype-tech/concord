// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_DAYCYCLESYSTEM_H
#define CONCORD_DAYCYCLESYSTEM_H

#include "Concord/CExport.h"
#include "engine/ecs/System.h"
#include "engine/scene/DayCycle.h"

namespace Concord {

/**
 * Advances a day cycle and writes it into the scene every frame.
 *
 * Register it with \code game.Systems().Add<DayCycleSystem>(settings)\endcode
 * and the scene's sun and sky follow the clock with no further wiring: it
 * rewrites the directional light's bearing, colour and intensity and the
 * environment's sky, ambient and cloud coverage, leaving every field it does
 * not own exactly as the scene authored it.
 */
class CENGINE_API DayCycleSystem : public ISystem {
public:
    explicit DayCycleSystem(const DayCycleSettings& settings = {}) noexcept;

    void OnUpdate(Scene& scene, f32 deltaTime) override;

    /** The clock, for a game that wants to read or set the time of day. */
    [[nodiscard]] const DayCycle& Cycle() const noexcept { return m_cycle; }

    /** The sky state most recently written into a scene. */
    [[nodiscard]] const SkyState& Sky() const noexcept { return m_sky; }

    /** Jumps the clock, which is how a game forces noon or dusk. */
    void SetHour(f32 hour) noexcept;

private:
    DayCycle m_cycle;
    SkyState m_sky{};
    /** Exposure the scene authored before the clock started dimming it. */
    f32 m_baseExposure = -1.0f;
};

} // namespace Concord

#endif // CONCORD_DAYCYCLESYSTEM_H
