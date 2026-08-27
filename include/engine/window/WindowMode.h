// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOWMODE_H
#define CONCORD_WINDOWMODE_H

namespace Concord {

/**
 * How a window is presented.
 *
 * `Borderless` is the recommended way to go full screen: it avoids the
 * display-mode switch `Fullscreen` performs, so alt-tabbing stays instant.
 */
enum class WindowMode {
    Windowed,
    Borderless,
    Fullscreen,
};

} // namespace Concord

#endif // CONCORD_WINDOWMODE_H
