// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CSCENE_H
#define CONCORD_CSCENE_H

/**
 * Public entry point for scenes, entity handles and environment settings.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/core/Transform.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/scene/EntityHandle.h"
#include "engine/scene/EnvironmentSettings.h"
#include "engine/scene/Scene.h"

#endif // CONCORD_CSCENE_H
