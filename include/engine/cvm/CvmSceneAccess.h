// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMSCENEACCESS_H
#define CONCORD_CVMSCENEACCESS_H

/**
 * Helpers shared by the units that implement the CVM C ABI.
 *
 * These used to be file-local in one translation unit; splitting the binding by
 * subject made them shared, and sharing them is better than each unit growing
 * its own slightly different idea of how a handle is resolved or a colour is
 * clamped.
 */

#include "engine/core/Color.h"
#include "engine/core/Transform.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/cvm/CvmEntityRegistry.h"
#include "engine/cvm/CvmSceneRegistry.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/World.h"
#include "engine/scene/EntityHandle.h"
#include "engine/scene/EnvironmentSettings.h"
#include "engine/scene/Scene.h"

#include <cmath>
#include <cstdint>

namespace Concord::Cvm {

/** Narrows three ABI doubles to the engine's single-precision vector. */
[[nodiscard]] inline Vec3 ToVec3(double x, double y, double z) noexcept
{
    return {static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(z)};
}

/** Clamps a colour channel into the 0..255 a channel byte can hold. */
[[nodiscard]] inline u8 Channel(std::int64_t value) noexcept
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<u8>(value);
}

/** Builds an albedo from three ABI channels. */
[[nodiscard]] inline ColorRGBA Colour(std::int64_t red, std::int64_t green,
                                      std::int64_t blue) noexcept
{
    return COLOR_RGB(Channel(red), Channel(green), Channel(blue));
}

/** World-space AABB centre and half-extent of a mesh primitive. */
[[nodiscard]] inline bool MeshBounds(Scene& scene, Entity entity, Vec3& centre,
                                     Vec3& half) noexcept
{
    Transform* transform = scene.GetWorld().Get<Transform>(entity);
    MeshRenderer* mesh = scene.GetWorld().Get<MeshRenderer>(entity);
    if (transform == nullptr || mesh == nullptr) return false;
    centre = transform->position;
    half.x = 0.5f * mesh->size.x * std::fabs(transform->scale.x);
    half.y = 0.5f * mesh->size.y * std::fabs(transform->scale.y);
    half.z = 0.5f * mesh->size.z * std::fabs(transform->scale.z);
    return true;
}

/** Resolves an entity handle together with the scene that owns it. */
[[nodiscard]] inline bool ResolveEntity(std::int64_t handle, Scene*& scene, Entity& entity)
{
    EntityRef ref;
    if (!AcquireEntity(handle, ref)) return false;
    Scene* owner = AcquireScene(ref.sceneHandle);
    // The pointer comparison is the point: a recycled scene handle resolves to
    // a different Scene, and this entity never belonged to it.
    if (owner == nullptr || owner != ref.scene) return false;
    scene = owner;
    entity = ref.entity;
    return true;
}

/** Runs \p fn over a live entity's transform, returning 1 when it ran. */
template <typename Fn>
std::int64_t EditTransform(std::int64_t handle, Fn&& fn)
{
    Scene* scene = nullptr;
    Entity entity;
    if (!ResolveEntity(handle, scene, entity)) return 0;
    Transform* transform = scene->GetWorld().Get<Transform>(entity);
    if (transform == nullptr) return 0;
    fn(*transform);
    return 1;
}

/** Runs \p fn over a component an entity carries, returning 1 when it ran. */
template <typename T, typename Fn>
std::int64_t EditComponent(std::int64_t handle, Fn&& fn)
{
    Scene* scene = nullptr;
    Entity entity;
    if (!ResolveEntity(handle, scene, entity)) return 0;
    T* component = scene->GetWorld().Get<T>(entity);
    if (component == nullptr) return 0;
    fn(*component);
    return 1;
}

/** Records a freshly spawned entity and returns the handle a script keeps. */
[[nodiscard]] inline std::int64_t Record(std::int64_t sceneHandle, Scene& scene,
                                         const EntityHandle& handle)
{
    return CreateEntity(sceneHandle, scene, handle.Id());
}

/** Reads, changes and writes back a scene's environment. */
template <typename Fn>
std::int64_t EditEnvironment(std::int64_t handle, Fn&& fn)
{
    Scene* found = AcquireScene(handle);
    if (found == nullptr) return 0;
    EnvironmentSettings settings = found->Environment();
    fn(settings);
    found->SetEnvironment(settings);
    return 1;
}

} // namespace Concord::Cvm

#endif // CONCORD_CVMSCENEACCESS_H
