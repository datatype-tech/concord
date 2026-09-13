// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/WaterSplashSystem.h"

#include "engine/core/Color.h"
#include "engine/core/Transform.h"
#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/WaterBodyComponent.h"
#include "engine/particle/ParticleTypes.h"
#include "engine/scene/ParticleEmitter.h"
#include "engine/scene/Scene.h"
#include "engine/scene/WaterRipple.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Packs an entity's slot and generation into one key stable across frames. */
u64 PackEntity(Entity entity) noexcept
{
    return (static_cast<u64>(entity.index) << 32) | static_cast<u64>(entity.generation);
}

/** Falling speed, in units per second, below which a crossing is too gentle to splash. */
constexpr f32 kMinimumSplashSpeed = 0.6f;

/** Falling speed at which a splash's ring and burst reach their full size. */
constexpr f32 kFullSplashSpeed = 6.0f;

/** Seconds a burst emitter is kept alive after spawning, covering its longest particle. */
constexpr f32 kBurstParticleLifetime = 0.55f;

} // namespace

void WaterSplashSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    World& world = scene.GetWorld();

    // Snapshotted once so the body query below tests against one frame's
    // worth of surfaces rather than re-querying water per dynamic body.
    m_surfaces.clear();
    world.Query<WaterBodyComponent, Transform>(
        [this](Entity, const WaterBodyComponent& body, const Transform& transform) {
            const f32 halfX = body.extent.x * std::abs(transform.scale.x);
            const f32 halfZ = body.extent.y * std::abs(transform.scale.z);
            // Either axis at zero opts a surface out entirely -- the sentinel
            // WaterBodyComponent itself documents for a vertical waterfall sheet.
            if (!(halfX > 0.0f) || !(halfZ > 0.0f)) {
                return;
            }
            m_surfaces.push_back(Surface{.worldY = transform.position.y,
                                          .centreX = transform.position.x,
                                          .centreZ = transform.position.z,
                                          .halfExtentX = halfX,
                                          .halfExtentZ = halfZ});
        });

    m_impacts.clear();
    if (!m_surfaces.empty()) {
        world.Query<RigidBody, Transform>(
            [this](Entity entity, const RigidBody& rigidBody, const Transform& transform) {
                if (rigidBody.motion != BodyMotion::Dynamic) {
                    return;
                }
                const Surface* hit = nullptr;
                for (const Surface& surface : m_surfaces) {
                    if (std::abs(transform.position.x - surface.centreX) <= surface.halfExtentX &&
                        std::abs(transform.position.z - surface.centreZ) <= surface.halfExtentZ) {
                        hit = &surface;
                        break;
                    }
                }
                bool& wasSubmerged = m_submerged[PackEntity(entity)];
                if (hit == nullptr) {
                    // Outside every footprint: neither above nor below anything
                    // in particular, so the next entry anywhere starts clean.
                    wasSubmerged = false;
                    return;
                }
                const bool below = transform.position.y <= hit->worldY;
                if (below && !wasSubmerged && rigidBody.linearVelocity.y < -kMinimumSplashSpeed) {
                    m_impacts.push_back(
                        Impact{.position = {transform.position.x, hit->worldY, transform.position.z},
                               .fallSpeed = -rigidBody.linearVelocity.y});
                }
                wasSubmerged = below;
            });
    }

    // Spawning is a structural change, so it happens after the query above
    // rather than inside it -- the same rule WaterRippleSystem's field
    // emission follows.
    for (const Impact& impact : m_impacts) {
        const f32 unit = std::clamp(
            (impact.fallSpeed - kMinimumSplashSpeed) / (kFullSplashSpeed - kMinimumSplashSpeed), 0.0f,
            1.0f);
        scene.Spawn<Object::WaterRipple>({
            .ripple = {.centre = {impact.position.x, impact.position.z},
                       .wavelength = 0.35f + unit * 0.5f,
                       .speed = 2.4f + unit * 3.2f,
                       .strength = 0.015f + unit * 0.10f,
                       .falloff = 0.55f,
                       .reach = 1.2f + unit * 2.4f},
            .lifetime = 0.7f + unit * 0.6f,
        });
        const Entity burst = scene.Spawn<Object::ParticleEmitter>(
            {.transform = {.position = impact.position},
             .settings = {.shape = ParticleShape::Cone,
                          .shapeExtent = {0.10f + unit * 0.20f, 0.05f, 0.0f},
                          // Continuous emission stays off: burstCount alone
                          // fires once on the emitter's first step, which is
                          // exactly the "one splash, then done" this needs.
                          .emissionRate = 0.0f,
                          .burstCount = static_cast<u32>(8.0f + unit * 26.0f),
                          .lifetime = {kBurstParticleLifetime * 0.6f, kBurstParticleLifetime},
                          .speed = {1.2f + unit * 1.6f, 2.4f + unit * 3.4f},
                          .startSize = {0.03f, 0.06f},
                          .endSize = {0.01f, 0.02f},
                          .direction = {0.0f, 1.0f, 0.0f},
                          .spreadDegrees = 48.0f,
                          .gravity = {0.0f, -14.0f, 0.0f},
                          .drag = 0.25f,
                          .startColor = COLOR_RGBA(226, 236, 244, 220),
                          .endColor = COLOR_RGBA(198, 212, 224, 0),
                          .looping = false,
                          .localSpace = false,
                          .capacity = 40,
                          .blend = ParticleBlendMode::Scatter}});
        m_bursts.emplace_back(burst, kBurstParticleLifetime + 0.2f);
    }

    for (std::pair<Entity, f32>& entry : m_bursts) {
        entry.second -= deltaTime;
    }
    m_bursts.erase(std::remove_if(m_bursts.begin(), m_bursts.end(),
                                  [&world](const std::pair<Entity, f32>& entry) {
                                      if (entry.second > 0.0f) {
                                          return false;
                                      }
                                      world.Destroy(entry.first);
                                      return true;
                                  }),
                   m_bursts.end());
}

} // namespace Concord
