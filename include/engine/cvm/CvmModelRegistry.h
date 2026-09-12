// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMMODELREGISTRY_H
#define CONCORD_CVMMODELREGISTRY_H

/**
 * Ownership of the model assets a CVM program has loaded.
 *
 * An imported model is decoded once and then referenced by any number of
 * entities, which is the arrangement the engine already wants: Object::Model
 * holds a shared_ptr because the geometry is not per-instance. A script holds
 * an integer, so the asset has to be owned somewhere the handle can find it --
 * here.
 *
 * Loading is the one place a CVM program touches the filesystem, so it is the
 * one place that can fail for a reason outside the program's control. A failed
 * load returns 0 rather than a handle to nothing.
 */

#include "engine/asset/ModelAsset.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Concord::Cvm {

/**
 * Decodes \p path and returns a handle, or 0 on failure.
 *
 * \p scale is applied uniformly by the importer. \p error receives the
 * loader's diagnostic when the load fails.
 */
std::int64_t LoadModel(const std::string& path, double scale, std::string& error);

/** Releases a model. Returns 1 on success, 0 when the handle is not live. */
std::int64_t FreeModel(std::int64_t handle);

/** Returns the asset behind a handle, or null when it is not live. */
std::shared_ptr<ModelAsset> AcquireModel(std::int64_t handle);

} // namespace Concord::Cvm

#endif // CONCORD_CVMMODELREGISTRY_H
