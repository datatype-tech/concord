// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_UIAPPEARANCE_H
#define CONCORD_UIAPPEARANCE_H
namespace Concord {
/** Dark themes supplied by Hello ImGui; rendering remains native to Concord. */
enum class UiToolkitTheme { SoDark, Darcula, Photoshop, Gray };
/** User scale multiplies the monitor DPI for both fonts and widget geometry. */
struct UiAppearance {
    UiToolkitTheme theme = UiToolkitTheme::Darcula;
    float scale = 1.0f;
    float rounding = 8.0f;
};
}
#endif
