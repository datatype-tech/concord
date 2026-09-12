// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMENTITYREGISTRY_H
#define CONCORD_CVMENTITYREGISTRY_H

/**
 * Ownership of the entity handles a CVM program holds.
 *
 * A spawn returns this handle, and a script keeps it in a global so it can move
 * the entity on later frames. The reference records the scene it came from as
 * well as the scene's handle: the handle alone is not enough, because a retired
 * handle is reused, and an entity read through a recycled scene would address a
 * slot that belongs to a different world.
 */

#include "engine/ecs/Entity.h"

#include <cstdint>

namespace Concord {
class Scene;
} // namespace Concord

namespace Concord::Cvm {

/** One live entity handle. */
struct EntityRef {
    /** Handle of the scene the entity belongs to. */
    std::int64_t sceneHandle = 0;

    /** The scene itself, so a recycled handle cannot be mistaken for this one. */
    Scene* scene = nullptr;

    /** The entity within that scene. */
    Entity entity{};
};

/** Records \p entity from \p scene and returns its handle, or 0 on failure. */
std::int64_t CreateEntity(std::int64_t sceneHandle, Scene& scene, Entity entity);

/**
 * Returns the existing handle for \p entity in \p scene, or records a new one.
 *
 * Spawn already records a handle; this is for discovering an entity the
 * script did not keep, such as the scene's current camera.
 */
std::int64_t FindOrCreateEntity(std::int64_t sceneHandle, Scene& scene, Entity entity);

/**
 * Copies the reference behind \p handle into \p out.
 *
 * Returns false when the handle is not live. The reference is copied rather
 * than returned by pointer so a concurrent creation cannot leave the caller
 * holding a pointer into storage that has moved.
 */
bool AcquireEntity(std::int64_t handle, EntityRef& out);

/** Retires \p handle. Returns 1 on success, 0 when it is not live. */
std::int64_t DestroyEntity(std::int64_t handle);

/**
 * Retires every handle belonging to \p sceneHandle.
 *
 * Called when a scene is destroyed, so a script that churns scenes does not
 * accumulate handles that can never resolve again.
 */
void ForgetScene(std::int64_t sceneHandle);

} // namespace Concord::Cvm

#endif // CONCORD_CVMENTITYREGISTRY_H
