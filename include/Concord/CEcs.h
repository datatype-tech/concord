// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CECS_H
#define CONCORD_CECS_H

/**
 * Public entry point for the entity-component-system layer.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/ecs/Components.h"
#include "engine/ecs/AnimationComponents.h"
#include "engine/ecs/AnimationSystem.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/System.h"
#include "engine/ecs/SystemSchedule.h"
#include "engine/ecs/World.h"
#include "engine/scene/EntityHandle.h"

#endif // CONCORD_CECS_H
