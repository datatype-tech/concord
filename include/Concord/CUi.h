// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CUI_H
#define CONCORD_CUI_H

/**
 * Public entry point for the immediate-mode game UI.
 *
 * This header only re-exports the real declarations from the engine module's
 * private headers (see AGENTS.md §3, facade re-export pattern); application
 * code includes this file, never the ones under `engine/`.
 */

#include "engine/ui/UiCanvas.h"
#include "engine/ui/UiDrawList.h"

#endif // CONCORD_CUI_H
