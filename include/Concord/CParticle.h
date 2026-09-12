// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CPARTICLE_H
#define CONCORD_CPARTICLE_H

/**
 * Public entry point for the emitter simulation.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md section 3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */
#include "engine/ecs/ParticleComponents.h"
#include "engine/ecs/ParticleSystem.h"
#include "engine/particle/ParticleSimulation.h"
#include "engine/particle/ParticleTypes.h"
#include "engine/scene/ParticleEmitter.h"

#endif // CONCORD_CPARTICLE_H
