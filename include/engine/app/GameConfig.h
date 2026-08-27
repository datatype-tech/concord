// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_GAMECONFIG_H
#define CONCORD_GAMECONFIG_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * Subsystem switches and frame-pacing policy for a Game.
 *
 * Following the on-demand activation rule (AGENTS.md §5), a subsystem is
 * only constructed when its flag is set, so a caller never pays for a
 * capability it did not ask for.
 */
struct GameConfig {
    /** Whether to bring up the Vulkan renderer. */
    bool enableRendering = true;

    /** Whether validation layers are requested; ignored in Release builds. */
    bool enableValidation = true;

    /**
     * Upper bound on frame rate when vsync is off, in frames per second.
     * Zero means unbounded.
     */
    u32 frameRateLimit = 0;
};

} // namespace Concord

#endif // CONCORD_GAMECONFIG_H
