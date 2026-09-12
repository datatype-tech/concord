// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERRIPPLESYSTEM_H
#define CONCORD_WATERRIPPLESYSTEM_H

#include "Concord/CExport.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/System.h"
#include "engine/ecs/WaterRippleComponents.h"

#include <vector>

namespace Concord {

/**
 * Owns every water disturbance's whole life: birth, ageing and retirement.
 *
 * Register it with `game.Systems().Add<WaterRippleSystem>()`. Ripples spawned
 * by hand through `Object::WaterRipple` are aged and retired by it, and so are
 * the ones a `WaterRippleFieldComponent` scatters. Keeping the two halves of a
 * lifetime in one place is what stops a field's rate and a ring's fade from
 * being tuned against two different clocks.
 *
 * A scene with no field and no hand-placed ring pays nothing beyond two empty
 * queries.
 */
class CENGINE_API WaterRippleSystem : public ISystem {
public:
    void OnUpdate(Scene& scene, f32 deltaTime) override;

private:
    /**
     * Entities to retire, kept between frames.
     *
     * Retiring has to happen after the query rather than inside it: creating or
     * destroying entities mid-iteration invalidates the dense arrays being
     * walked. Holding the scratch vector as a member keeps a steady stream of
     * ripples from allocating every frame.
     */
    std::vector<Entity> m_retired;

    /**
     * Fields and their owners, gathered before spawning from them.
     *
     * Spawning is a structural change, so the settings are copied out first and
     * written back afterwards; a steady scene therefore allocates nothing.
     */
    std::vector<WaterRippleFieldComponent> m_fields;
    std::vector<Entity> m_owners;
};

} // namespace Concord

#endif // CONCORD_WATERRIPPLESYSTEM_H
