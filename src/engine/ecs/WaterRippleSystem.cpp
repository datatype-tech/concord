// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/WaterRippleSystem.h"

#include "engine/ecs/WaterRippleComponents.h"
#include "engine/particle/ParticleSampling.h"
#include "engine/scene/Scene.h"
#include "engine/scene/WaterRipple.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Most rings one field may release in a single frame. */
constexpr f32 kMaximumBurst = 8.0f;

/** One deterministic sample in 0..1 from a mutable small-integer state. */
f32 NextUnit(u32& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return static_cast<f32>((state >> 8) & 0xFFFFu) / 65535.0f;
}

/** One sample from a range, tolerant of a range authored backwards. */
f32 SampleRange(u32& state, const ParticleRange& range) noexcept
{
    const f32 low = std::isfinite(range.min) ? range.min : 0.0f;
    const f32 high = std::isfinite(range.max) ? range.max : low;
    const f32 lo = std::min(low, high);
    const f32 hi = std::max(low, high);
    return lo + (hi - lo) * NextUnit(state);
}

/** Spawns every ring a field owes for the delta it has just been given. */
void EmitFromFields(Scene& scene, std::vector<WaterRippleFieldComponent>& scratch,
                    std::vector<Entity>& owners, f32 step)
{
    scratch.clear();
    owners.clear();
    scene.Query<WaterRippleFieldComponent>(
        [&](Entity entity, WaterRippleFieldComponent& field) {
            scratch.push_back(field);
            owners.push_back(entity);
        });

    for (usize index = 0; index < scratch.size(); ++index) {
        WaterRippleFieldComponent field = scratch[index];
        const WaterRippleFieldSettings& settings = field.settings;
        if (!settings.enabled || !(settings.rate > 0.0f) || step <= 0.0f) {
            continue;
        }
        // The debt is counted in rings, not in seconds: a rate of two per
        // second owes two tenths of a ring over a tenth of a second, and each
        // ring it pays for costs exactly one. Carrying the debt in seconds and
        // dividing by an interval double-counts the rate.
        field.pending += settings.rate * step;
        // Capped so a long stall releases a handful of rings rather than the
        // hundred it accumulated, which would read as a burst and not as rain.
        u32 budget = static_cast<u32>(std::min(field.pending, kMaximumBurst));
        field.pending = std::min(field.pending, kMaximumBurst);
        while (budget-- != 0u) {
            field.pending = std::max(field.pending - 1.0f, 0.0f);
            const f32 halfX = std::abs(settings.extent.x);
            const f32 halfZ = std::abs(settings.extent.y);
            WaterRipple ring{};
            ring.centre = {(NextUnit(field.state) * 2.0f - 1.0f) * halfX,
                           (NextUnit(field.state) * 2.0f - 1.0f) * halfZ};
            ring.wavelength = std::max(SampleRange(field.state, settings.wavelength), 0.05f);
            ring.speed = SampleRange(field.state, settings.speed);
            ring.strength = std::max(SampleRange(field.state, settings.strength), 0.0f);
            ring.falloff = std::max(settings.falloff, 0.0f);
            ring.reach = std::max(settings.reach, 0.05f);
            scene.Spawn<Object::WaterRipple>(
                {.ripple = ring,
                 .lifetime = std::max(SampleRange(field.state, settings.lifetime), 0.01f)});
        }
        if (WaterRippleFieldComponent* stored = scene.GetWorld().Get<WaterRippleFieldComponent>(owners[index])) {
            *stored = field;
        }
    }
}

} // namespace

void WaterRippleSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    const f32 step = std::isfinite(deltaTime) && deltaTime > 0.0f ? deltaTime : 0.0f;

    // Birth first, then age, then retire. All three live here because a ring's
    // life is one thing: splitting the spawning into its own system would put
    // the two halves of the same lifetime in two places that can disagree.
    EmitFromFields(scene, m_fields, m_owners, step);

    m_retired.clear();
    scene.Query<WaterRippleComponent>([this, step](Entity entity, WaterRippleComponent& ripple) {
        ripple.age += step;
        if (ripple.age >= ripple.lifetime) {
            m_retired.push_back(entity);
        }
    });
    // Through the store rather than through a Scene wrapper: destroying is a
    // structural change, and the world is the thing that owns entity lifetime.
    for (const Entity entity : m_retired) {
        scene.GetWorld().Destroy(entity);
    }
    m_retired.clear();
}

} // namespace Concord
