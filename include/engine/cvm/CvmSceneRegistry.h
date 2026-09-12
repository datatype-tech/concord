// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMSCENEREGISTRY_H
#define CONCORD_CVMSCENEREGISTRY_H

/**
 * Ownership of the scenes a CVM program has created.
 *
 * A CVM program holds an integer, not an object, so the engine has to own the
 * Scene somewhere the handle can find it again. Handles are slot indices plus
 * one, which keeps zero permanently invalid and lets a retired slot be reused.
 *
 * The table is locked, so creating and destroying scenes from different threads
 * is safe. Using a scene from two threads at once is not, and is the caller's
 * problem exactly as it would be in C++.
 */

#include <cstdint>

namespace Concord {
class Scene;
} // namespace Concord

namespace Concord::Cvm {

/** Creates an empty scene. Returns a handle, or 0 when allocation fails. */
std::int64_t CreateScene();

/** Releases a scene. Returns 1 on success, 0 when the handle is not live. */
std::int64_t DestroyScene(std::int64_t handle);

/** Returns the scene behind a handle, or null when it is not live. */
Scene* AcquireScene(std::int64_t handle);

} // namespace Concord::Cvm

#endif // CONCORD_CVMSCENEREGISTRY_H
