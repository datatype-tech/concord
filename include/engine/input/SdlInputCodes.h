// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_INPUT_SDLINPUTCODES_H
#define CONCORD_INPUT_SDLINPUTCODES_H

#include "engine/input/InputSnapshot.h"

#include <SDL3/SDL_scancode.h>

namespace Concord {

/** Maps an SDL scancode to its KeyCode, or KeyCode::None when unmapped. */
[[nodiscard]] KeyCode KeyCodeFromSdlScanCode(SDL_Scancode scancode) noexcept;

/**
 * Maps an SDL mouse-button number to its MouseButton.
 *
 * @return False for buttons the engine does not model.
 */
[[nodiscard]] bool MouseButtonFromSdlButton(int button, MouseButton& out) noexcept;

} // namespace Concord

#endif // CONCORD_INPUT_SDLINPUTCODES_H
