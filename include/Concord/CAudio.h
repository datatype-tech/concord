// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CAUDIO_H
#define CONCORD_CAUDIO_H

/**
 * Public entry point for the Steam Audio spatial mixer.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/audio/AudioClip.h"
#include "engine/audio/AudioSettings.h"
#include "engine/audio/AudioWorld.h"
#include "engine/ecs/AudioComponents.h"
#include "engine/ecs/AudioSystem.h"
#include "engine/scene/Sound.h"

#endif // CONCORD_CAUDIO_H
