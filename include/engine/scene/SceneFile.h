// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SCENEFILE_H
#define CONCORD_SCENEFILE_H

#include "Concord/CExport.h"
#include "engine/scene/Scene.h"

namespace Concord {

/**
 * Loads a scene document when the process environment names one.
 *
 * `CONCORD_SCENE` is a UTF-16 path to a `CONCORD_SCENE` 1–3 file, the same
 * document the editor writes. The running game reads that file instead of the
 * layout compiled into the executable, so a scene edit can launch without a
 * rebuild. When the variable is unset, `loaded` is false and the scene is
 * left untouched.
 *
 * @param loaded Set when an override path was present.
 * @return True when the loaded document contains a static or dynamic body.
 */
CENGINE_API bool LoadSceneOverride(Scene& scene, bool& loaded);

} // namespace Concord

#endif // CONCORD_SCENEFILE_H
