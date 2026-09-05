// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANGLE_H
#define CONCORD_ANGLE_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * Converts degrees to radians.
 *
 * Desc fields facing the application are spelled in degrees because that is
 * what a human writes by hand; everything below the API boundary works in
 * radians, and this is the single conversion point between the two.
 */
[[nodiscard]] constexpr f32 Radians(f32 degrees) noexcept
{
    return degrees * 0.017453292519943295f;
}

/** Converts radians back to degrees, the inverse of Radians(). */
[[nodiscard]] constexpr f32 Degrees(f32 radians) noexcept
{
    return radians * 57.29577951308232f;
}

} // namespace Concord

#endif // CONCORD_ANGLE_H
