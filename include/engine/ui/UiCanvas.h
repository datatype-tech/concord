// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_UICANVAS_H
#define CONCORD_UICANVAS_H

#include "Concord/CExport.h"
#include "engine/ui/UiDrawList.h"

#include <array>

#include <array>

namespace Concord {

/** Colours and metrics shared by every widget on one canvas. */
struct UiTheme {
    std::array<f32, 4> panel{0.06f, 0.07f, 0.10f, 0.78f};
    std::array<f32, 4> button{0.16f, 0.18f, 0.24f, 0.92f};
    std::array<f32, 4> buttonHot{0.24f, 0.28f, 0.38f, 0.96f};
    std::array<f32, 4> text{1.0f, 1.0f, 1.0f, 1.0f};
    f32 lineHeight = 18.0f;
};

/**
 * Immediate-mode UI canvas.
 *
 * Call Begin() once per frame with the live pointer, emit widgets, then End().
 * The draw list is valid until the next Begin().
 */
class CENGINE_API UiCanvas {
public:
    void Begin(f32 mouseX, f32 mouseY, bool mousePressed, f32 width, f32 height) noexcept;
    void End() noexcept;

    void Panel(f32 x, f32 y, f32 width, f32 height) noexcept;
    void Label(f32 x, f32 y, const char* text) noexcept;
    [[nodiscard]] bool Button(f32 x, f32 y, f32 width, f32 height, const char* text) noexcept;

    [[nodiscard]] const UiDrawList& DrawList() const noexcept { return m_list; }
    [[nodiscard]] bool IsOpen() const noexcept { return m_open; }

    UiTheme theme{};

private:
    UiDrawList m_list{};
    f32 m_mouseX = 0.0f;
    f32 m_mouseY = 0.0f;
    bool m_mousePressed = false;
    f32 m_width = 0.0f;
    f32 m_height = 0.0f;
    bool m_open = false;
};

} // namespace Concord

#endif // CONCORD_UICANVAS_H
