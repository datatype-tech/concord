// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOWACCESS_H
#define CONCORD_WINDOWACCESS_H

#include "Concord/CExport.h"

namespace Concord {

class Window;

/** Internal bridge for renderer code that needs the opaque native handle. */
struct CENGINE_API WindowAccess {
    [[nodiscard]] static void* NativeHandle(const Window& window) noexcept;
    [[nodiscard]] static bool ConsumeResizeFlag(Window& window) noexcept;
};

} // namespace Concord

#endif // CONCORD_WINDOWACCESS_H
