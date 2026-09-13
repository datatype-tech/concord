// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CWATER_H
#define CONCORD_CWATER_H

/**
 * Public entry point for authored water surfaces.
 *
 * This header only re-exports the real declarations from the engine module's
 * private headers (see AGENTS.md §3, facade re-export pattern); application
 * code includes this file, never the ones under `engine/`.
 */

#include "engine/asset/WaterMaterial.h"
#include "engine/ecs/WaterRippleSystem.h"
#include "engine/ecs/WaterSplashSystem.h"
#include "engine/scene/Water.h"
#include "engine/scene/WaterRipple.h"

#endif // CONCORD_CWATER_H
