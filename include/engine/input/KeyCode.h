// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_INPUT_KEYCODE_H
#define CONCORD_INPUT_KEYCODE_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * Platform-independent keyboard key codes.
 *
 * Layout codes describe the physical US-layout key, not the produced
 * character, so bindings survive keyboard layouts and input methods. The
 * enumeration doubles as a dense index into InputSnapshot's key array.
 */
enum class KeyCode : u16 {
    None = 0,

    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Digit0,

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    Up,
    Down,
    Left,
    Right,

    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    LeftSystem,
    RightSystem,

    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    CapsLock,
    NumLock,
    PrintScreen,
    ScrollLock,
    Pause,
    Application,

    Grave,
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon,
    Apostrophe,
    Comma,
    Period,
    Slash,
    NonUsBackslash,

    KeyPad0,
    KeyPad1,
    KeyPad2,
    KeyPad3,
    KeyPad4,
    KeyPad5,
    KeyPad6,
    KeyPad7,
    KeyPad8,
    KeyPad9,
    KeyPadDivide,
    KeyPadMultiply,
    KeyPadMinus,
    KeyPadPlus,
    KeyPadEnter,
    KeyPadPeriod,
};

inline constexpr u32 kKeyCodeCount = static_cast<u32>(KeyCode::KeyPadPeriod) + 1;

} // namespace Concord

#endif // CONCORD_INPUT_KEYCODE_H
