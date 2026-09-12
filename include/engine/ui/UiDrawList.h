// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_UIDRAWLIST_H
#define CONCORD_UIDRAWLIST_H

#include "engine/core/Types.h"

#include <array>
#include <cstddef>
#include <vector>

namespace Concord {

/** Largest label a single UI command can carry, including the terminator. */
inline constexpr std::size_t kUiTextCapacity = 96;

/** One immediate-mode draw command produced by a UiCanvas. */
enum class UiDrawKind : u8 {
    Rect = 0,
    Text = 1,
};

struct UiDrawCommand {
    UiDrawKind kind = UiDrawKind::Rect;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
    std::array<f32, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    char text[kUiTextCapacity] = {};
};

/**
 * Retained list of UI primitives for one frame.
 *
 * Cleared at the start of each Begin(). The renderer reads it after End().
 */
struct UiDrawList {
    std::vector<UiDrawCommand> commands;
};

} // namespace Concord

#endif // CONCORD_UIDRAWLIST_H
