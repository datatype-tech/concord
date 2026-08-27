// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CORE_TYPES_H
#define CONCORD_CORE_TYPES_H

/** Fundamental scalar type aliases. The core layer's bedrock; depends on no other engine module. */

#include <cstddef>
#include <cstdint>

namespace Concord {

using i8    = std::int8_t;
using i16   = std::int16_t;
using i32   = std::int32_t;
using i64   = std::int64_t;
using u8    = std::uint8_t;
using u16   = std::uint16_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using f32   = float;
using f64   = double;
using usize = std::size_t;

} // namespace Concord

#endif // CONCORD_CORE_TYPES_H
