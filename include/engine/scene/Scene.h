// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SCENE_H
#define CONCORD_SCENE_H

#include "engine/core/Types.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"
#include "engine/scene/Box.h"
#include "engine/scene/Camera.h"
#include "engine/scene/EntityHandle.h"
#include "engine/scene/EnvironmentSettings.h"
#include "engine/scene/SunLight.h"

#include <utility>

namespace Concord {

/**
 * A world of entities, presented through two interchangeable views.
 *
 * The object-oriented view spawns typed archetypes and gets a handle back:
 * `scene.Spawn<Object::Box>({...})`. The data-oriented view walks the same
 * storage directly: `scene.Query<Transform, MeshRenderer>(...)`. They are
 * two faces of one component store, so a change made through either is
 * immediately visible to the other. A Scene is non-movable so its handles
 * always retain a stable World address.
 */
class Scene {
public:
    Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    /**
     * Creates an entity of archetype `T` from its descriptor.
     *
     * `T::Desc` is deduced, so the call site only writes braces:
     * `scene.Spawn<Object::Box>({.transform = {...}})`.
     *
     * @return A handle that can chain further Add/Remove calls.
     */
    template <typename T>
    EntityHandle Spawn(typename T::Desc desc)
    {
        const Entity entity = m_world.Create();
        try {
            T::Build(m_world, entity, desc);
        } catch (...) {
            try {
                m_world.Destroy(entity);
            } catch (...) {
                // Preserve the archetype's construction failure for the caller.
            }
            throw;
        }
        return EntityHandle{m_world, entity};
    }

    /**
     * Creates a bare entity carrying no components.
     *
     * The starting point for composing an archetype the engine does not
     * ship, without going through Spawn.
     */
    EntityHandle CreateEntity()
    {
        return EntityHandle{m_world, m_world.Create()};
    }

    /** Wraps an existing raw handle for object-style access. */
    [[nodiscard]] EntityHandle Wrap(Entity entity) noexcept
    {
        return EntityHandle{m_world, entity};
    }

    /**
     * Invokes `fn(Entity, T&, Rest&...)` for every entity carrying all of
     * the named components. Name the rarest component first.
     */
    template <typename T, typename... Rest, typename Fn>
    void Query(Fn&& fn)
    {
        m_world.Query<T, Rest...>(std::forward<Fn>(fn));
    }

    /** Const query view exposing borrowed components as const references. */
    template <typename T, typename... Rest, typename Fn>
    void Query(Fn&& fn) const
    {
        m_world.Query<T, Rest...>(std::forward<Fn>(fn));
    }

    /** The component store, for systems that need the full ECS surface. */
    [[nodiscard]] World& GetWorld() noexcept { return m_world; }
    [[nodiscard]] const World& GetWorld() const noexcept { return m_world; }

    /** Applies structural commands queued by Query callbacks. */
    usize FlushDeferred() { return m_world.FlushDeferred(); }

    /**
     * The entity the scene renders through: the camera with the lowest
     * priority that also has a Transform, or an invalid handle when no
     * complete camera was spawned.
     */
    [[nodiscard]] Entity MainCamera() const noexcept
    {
        Entity best = kInvalidEntity;
        i32 bestPriority = 0;

        m_world.Query<CameraComponent, Transform>(
            [&](Entity entity, const CameraComponent& camera, const Transform&) {
                if (!best.IsValid() || camera.priority < bestPriority) {
                    best = entity;
                    bestPriority = camera.priority;
                }
            });
        return best;
    }

    /** Sky and ambient settings. */
    [[nodiscard]] const EnvironmentSettings& Environment() const noexcept { return m_environment; }

    /** Replaces the sky and ambient settings wholesale. */
    void SetEnvironment(EnvironmentSettings settings) noexcept { m_environment = settings; }

    /** Number of live entities. */
    [[nodiscard]] usize EntityCount() const noexcept { return m_world.EntityCount(); }

private:
    World m_world;
    EnvironmentSettings m_environment{};
};

} // namespace Concord

#endif // CONCORD_SCENE_H
