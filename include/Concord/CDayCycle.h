// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CDAYCYCLE_H
#define CONCORD_CDAYCYCLE_H

/**
 * Public entry point for the day and night cycle.
 *
 * This header only re-exports the real declarations from the engine module's
 * private headers (see AGENTS.md §3, facade re-export pattern); application
 * code includes this file, never the ones under `engine/`.
 */

#include "engine/ecs/DayCycleSystem.h"
#include "engine/scene/DayCycle.h"

#endif // CONCORD_CDAYCYCLE_H
