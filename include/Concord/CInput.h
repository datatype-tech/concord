// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CINPUT_H
#define CONCORD_CINPUT_H

/**
 * Public entry point for keyboard, mouse, and action-mapping input.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/input/InputMap.h"
#include "engine/input/InputSnapshot.h"
#include "engine/input/KeyCode.h"

#endif // CONCORD_CINPUT_H
