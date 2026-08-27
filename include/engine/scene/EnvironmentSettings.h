// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ENVIRONMENTSETTINGS_H
#define CONCORD_ENVIRONMENTSETTINGS_H

#include "engine/core/Color.h"
#include "engine/core/Types.h"

namespace Concord {

/** Sky and ambient lighting for a scene. */
struct EnvironmentSettings {
    /** Color the framebuffer is cleared to, standing in for the sky. */
    ColorRGBA skyColor = COLOR_RGB(38, 48, 66);

    /** Ambient light color. */
    ColorRGBA ambientColor = COLOR_RGB(148, 168, 209);

    /** Ambient light strength. */
    f32 ambientIntensity = 0.4f;
};

} // namespace Concord

#endif // CONCORD_ENVIRONMENTSETTINGS_H
