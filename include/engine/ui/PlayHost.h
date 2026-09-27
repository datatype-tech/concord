// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_PLAYHOST_H
#define CONCORD_PLAYHOST_H

#include "Concord/CExport.h"

namespace Concord {
class Window;

/**
 * Concord bar around a game launched from the editor.
 *
 * Active while the process environment contains CONCORD_PLAY_HOST and the
 * session has not switched to a direct window. The game draws its image under
 * the bar; Dismiss expands the image to the full window for the rest of the
 * process. Build does not set the variable.
 */
struct CENGINE_API PlayHost {
    [[nodiscard]] static bool Active();
    /** Height of the bar in the current ImGui frame, or 32 before a frame exists. */
    [[nodiscard]] static float BarHeight();
    static void Dismiss();
    /** Draws the bar when Active and an ImGui frame is current. */
    static void Draw(Window& window);
};
}
#endif
