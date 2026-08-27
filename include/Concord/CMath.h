// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CMATH_H
#define CONCORD_CMATH_H

/**
 * Public entry point for vectors, matrices and angle helpers.
 *
 * This header only re-exports the real declarations from the engine
 * module's private headers (see AGENTS.md §3, facade re-export pattern);
 * application code includes this file, never the ones under `engine/`.
 */

#include "engine/core/Angle.h"
#include "engine/core/Mat4.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/core/Vec4.h"

#endif // CONCORD_CMATH_H
