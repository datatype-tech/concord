// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ui/UiCanvas.h"

#include <cstring>

namespace {

bool TestHiddenCanvasEmitsNothing()
{
    Concord::UiCanvas canvas;
    canvas.Panel(0.0f, 0.0f, 40.0f, 20.0f);
    canvas.Label(0.0f, 0.0f, "hi");
    return !canvas.IsOpen() && canvas.DrawList().commands.empty();
}

bool TestButtonClickAndMiss()
{
    Concord::UiCanvas canvas;
    canvas.Begin(12.0f, 12.0f, true, 200.0f, 100.0f);
    const bool hit = canvas.Button(8.0f, 8.0f, 40.0f, 16.0f, "Day");
    const bool miss = canvas.Button(80.0f, 8.0f, 40.0f, 16.0f, "Night");
    canvas.End();
    if (!hit || miss || canvas.DrawList().commands.size() < 4) {
        return false;
    }
    return std::strcmp(canvas.DrawList().commands[1].text, "Day") == 0;
}

bool TestBeginClearsPreviousFrame()
{
    Concord::UiCanvas canvas;
    canvas.Begin(0.0f, 0.0f, false, 100.0f, 100.0f);
    canvas.Label(0.0f, 0.0f, "one");
    canvas.End();
    canvas.Begin(0.0f, 0.0f, false, 100.0f, 100.0f);
    canvas.Label(0.0f, 0.0f, "two");
    canvas.End();
    return canvas.DrawList().commands.size() == 1 &&
           std::strcmp(canvas.DrawList().commands[0].text, "two") == 0;
}

} // namespace

int main()
{
    return TestHiddenCanvasEmitsNothing() && TestButtonClickAndMiss() &&
                   TestBeginClearsPreviousFrame()
               ? 0
               : 1;
}
