// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CAPPLICATION_H
#define CONCORD_CAPPLICATION_H

/**
 * Public entry point for the engine lifecycle and windowing.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/app/Game.h"
#include "engine/app/GameConfig.h"
#include "engine/window/Resolution.h"
#include "engine/window/Window.h"
#include "engine/window/WindowDesc.h"
#include "engine/window/WindowMode.h"

#endif // CONCORD_CAPPLICATION_H
