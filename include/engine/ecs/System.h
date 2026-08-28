// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SYSTEM_H
#define CONCORD_SYSTEM_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

namespace Concord {

class Scene;

/**
 * A unit of per-frame behaviour that operates on components.
 *
 * Systems are where game logic lives in the data-oriented view: rather than
 * each object updating itself, a system queries the components it cares
 * about and updates them in bulk.
 */
class CENGINE_API ISystem {
public:
    virtual ~ISystem() = default;

    /** Called once when the system is registered with a running scene. */
    virtual void OnStart(Scene&) {}

    /**
     * Called once per frame.
     *
     * @param deltaTime Seconds elapsed since the previous frame.
     */
    virtual void OnUpdate(Scene& scene, f32 deltaTime) = 0;

    /** Called once when Game explicitly stops the active scene. */
    virtual void OnStop(Scene&) {}
};

} // namespace Concord

#endif // CONCORD_SYSTEM_H
