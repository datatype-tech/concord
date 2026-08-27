// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VEC4_H
#define CONCORD_VEC4_H

#include "engine/core/Types.h"

namespace Concord {

/** Four-component vector, used for matrix columns and homogeneous coordinates. */
struct Vec4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    [[nodiscard]] f32& operator[](usize i) noexcept { return (&x)[i]; }
    [[nodiscard]] const f32& operator[](usize i) const noexcept { return (&x)[i]; }
};

} // namespace Concord

#endif // CONCORD_VEC4_H
