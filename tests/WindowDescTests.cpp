// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/Window.h"

#include <string>

int main()
{
    Concord::Window window;
    if (window.IsOpen() || window.Title() != "Concord Flash" || window.Width() != 1280 ||
        window.Height() != 720 || window.Mode() != Concord::WindowMode::Windowed ||
        !window.Vsync()) {
        return 1;
    }

    window.Set({
        .title = "Configured",
        .resolution = {.width = 640, .height = 360},
        .mode = Concord::WindowMode::Borderless,
        .resizable = false,
        .visible = false,
        .vsync = false,
    });
    if (window.Title() != "Configured" || window.Width() != 640 || window.Height() != 360 ||
        window.Mode() != Concord::WindowMode::Borderless || window.Vsync()) {
        return 1;
    }

    window.SetTitle(std::string("Renamed"));
    window.SetMode(Concord::WindowMode::Fullscreen);
    return window.Title() == "Renamed" && window.Mode() == Concord::WindowMode::Fullscreen ? 0
                                                                                             : 1;
}
