// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_INPUT_INPUTSNAPSHOT_H
#define CONCORD_INPUT_INPUTSNAPSHOT_H

#include "engine/core/Types.h"
#include "engine/core/Vec2.h"
#include "engine/input/KeyCode.h"

#include <array>

namespace Concord {

/** Per-button state bits stored in InputSnapshot's arrays. */
inline constexpr u8 kInputStateDown = 1u << 0;
/** Set on the pump that observed the transition into Down. */
inline constexpr u8 kInputStatePressed = 1u << 1;
/** Set on the pump that observed the transition out of Down. */
inline constexpr u8 kInputStateReleased = 1u << 2;

/** Mouse buttons, ordered so the enumeration indexes InputSnapshot directly. */
enum class MouseButton : u8 {
    Left,
    Right,
    Middle,
    X1,
    X2,
};

inline constexpr u32 kMouseButtonCount = static_cast<u32>(MouseButton::X2) + 1;

/**
 * One pump's worth of keyboard and mouse state.
 *
 * Filled by the window event pump: Down/Pressed/Released bits describe every
 * key and mouse button, and the vector fields accumulate motion and wheel
 * events since the previous pump. A plain aggregate, so tests can build one
 * by hand without a window.
 */
struct InputSnapshot {
    std::array<u8, kKeyCodeCount> keys{};
    std::array<u8, kMouseButtonCount> mouseButtons{};

    /** Cursor position in window-client coordinates from the last motion event. */
    Vec2 mousePosition{};
    /** Accumulated relative motion, only collected while the cursor is captured. */
    Vec2 mouseDelta{};
    /** Accumulated wheel motion; positive y scrolls up. */
    Vec2 wheelDelta{};
};

/** Transitions one key into or out of Down, recording the edge exactly once. */
inline void SetKeyState(InputSnapshot& snapshot, KeyCode code, bool down) noexcept
{
    if (code == KeyCode::None) return;
    u8& state = snapshot.keys[static_cast<u32>(code)];
    if (down) {
        if (!(state & kInputStateDown)) state |= kInputStateDown | kInputStatePressed;
    } else if (state & kInputStateDown) {
        state = kInputStateReleased;
    }
}

/** Transitions one mouse button into or out of Down, recording the edge. */
inline void SetButtonState(InputSnapshot& snapshot, MouseButton button, bool down) noexcept
{
    u8& state = snapshot.mouseButtons[static_cast<u32>(button)];
    if (down) {
        if (!(state & kInputStateDown)) state |= kInputStateDown | kInputStatePressed;
    } else if (state & kInputStateDown) {
        state = kInputStateReleased;
    }
}

/** Drops frame edges while retaining Down state, and resets accumulated axes. */
inline void BeginInputFrame(InputSnapshot& snapshot) noexcept
{
    for (u8& state : snapshot.keys) state &= kInputStateDown;
    for (u8& state : snapshot.mouseButtons) state &= kInputStateDown;
    snapshot.mouseDelta = {};
    snapshot.wheelDelta = {};
}

/** Releases everything currently held, as a focus-loss event requires. */
inline void ReleaseAllInput(InputSnapshot& snapshot) noexcept
{
    for (u8& state : snapshot.keys) {
        state = (state & kInputStateDown) ? kInputStateReleased : u8{0};
    }
    for (u8& state : snapshot.mouseButtons) {
        state = (state & kInputStateDown) ? kInputStateReleased : u8{0};
    }
    snapshot.mouseDelta = {};
}

} // namespace Concord

#endif // CONCORD_INPUT_INPUTSNAPSHOT_H
