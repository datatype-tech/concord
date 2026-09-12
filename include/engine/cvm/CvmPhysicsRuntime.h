// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMPHYSICSRUNTIME_H
#define CONCORD_CVMPHYSICSRUNTIME_H

namespace Concord {
class Scene;
} // namespace Concord

namespace Concord::Cvm {

/**
 * Drops a CVM-owned PhysicsSystem bound to \p scene.
 *
 * Used when the scene is destroyed, and again just before RunScene registers
 * the Game's PhysicsSystem, so two solvers never share one Scene.
 */
void ReleaseSimulation(Scene& scene);

} // namespace Concord::Cvm

#endif // CONCORD_CVMPHYSICSRUNTIME_H
