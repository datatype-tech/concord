// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CDEBUG_H
#define CONCORD_CDEBUG_H

/**
 * Public surface of the engine's debug facilities.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/debug/DebugOverlay.h"
#include "engine/debug/DebugOverlayFrame.h"

#endif // CONCORD_CDEBUG_H
