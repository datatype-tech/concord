// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/input/InputMap.h"
#include "engine/input/SdlInputCodes.h"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <cstdio>

namespace {

enum TestAction : Concord::u32 {
    ActionForward = 1,
    ActionJump = 2,
    ActionFire = 3,
};

Concord::InputSnapshot KeyFrame(Concord::KeyCode code, bool down)
{
    Concord::InputSnapshot snapshot{};
    Concord::SetKeyState(snapshot, code, down);
    return snapshot;
}

Concord::InputSnapshot ButtonFrame(Concord::MouseButton button, bool down)
{
    Concord::InputSnapshot snapshot{};
    Concord::SetButtonState(snapshot, button, down);
    return snapshot;
}

bool TestSnapshotTrinity()
{
    Concord::InputSnapshot snapshot = KeyFrame(Concord::KeyCode::W, true);
    const Concord::u8 held = snapshot.keys[static_cast<Concord::u32>(Concord::KeyCode::W)];
    if (!((held & Concord::kInputStateDown) && (held & Concord::kInputStatePressed))) {
        return false;
    }
    Concord::SetKeyState(snapshot, Concord::KeyCode::W, true);
    const Concord::u8 repeated = snapshot.keys[static_cast<Concord::u32>(Concord::KeyCode::W)];
    if (!(repeated & Concord::kInputStatePressed)) return false;

    Concord::SetKeyState(snapshot, Concord::KeyCode::W, false);
    const Concord::u8 released = snapshot.keys[static_cast<Concord::u32>(Concord::KeyCode::W)];
    if ((released & Concord::kInputStateDown) || !(released & Concord::kInputStateReleased)) {
        return false;
    }

    Concord::BeginInputFrame(snapshot);
    if (snapshot.keys[static_cast<Concord::u32>(Concord::KeyCode::W)] != 0) return false;

    snapshot = KeyFrame(Concord::KeyCode::W, true);
    Concord::ReleaseAllInput(snapshot);
    const Concord::u8 lost = snapshot.keys[static_cast<Concord::u32>(Concord::KeyCode::W)];
    return (lost & Concord::kInputStateReleased) != 0 && !(lost & Concord::kInputStateDown);
}

bool TestActionEdges()
{
    Concord::InputMap map;
    map.BindKey(ActionForward, Concord::KeyCode::W);
    if (!map.IsBound(ActionForward) || map.IsDown(ActionForward)) return false;

    map.Update(KeyFrame(Concord::KeyCode::W, true));
    if (!map.IsDown(ActionForward) || !map.WasPressed(ActionForward)) return false;

    map.Update(KeyFrame(Concord::KeyCode::W, true));
    if (!map.IsDown(ActionForward) || map.WasPressed(ActionForward)) return false;

    map.Update(KeyFrame(Concord::KeyCode::W, false));
    if (map.IsDown(ActionForward) || !map.WasReleased(ActionForward)) return false;

    map.Update(KeyFrame(Concord::KeyCode::W, false));
    return !map.WasReleased(ActionForward);
}

bool TestMultiBindingOr()
{
    Concord::InputMap map;
    map.BindKey(ActionForward, Concord::KeyCode::W);
    map.BindKey(ActionForward, Concord::KeyCode::Up);

    Concord::InputSnapshot snapshot = KeyFrame(Concord::KeyCode::W, true);
    map.Update(snapshot);
    if (!map.IsDown(ActionForward)) return false;

    Concord::SetKeyState(snapshot, Concord::KeyCode::Up, true);
    map.Update(snapshot);
    if (!map.IsDown(ActionForward) || map.WasPressed(ActionForward)) return false;

    Concord::SetKeyState(snapshot, Concord::KeyCode::W, false);
    map.Update(snapshot);
    if (!map.IsDown(ActionForward) || map.WasReleased(ActionForward)) return false;

    Concord::SetKeyState(snapshot, Concord::KeyCode::Up, false);
    map.Update(snapshot);
    return map.WasReleased(ActionForward) && !map.IsDown(ActionForward);
}

bool TestRuntimeRebinding()
{
    Concord::InputMap map;
    map.BindKey(ActionForward, Concord::KeyCode::W);
    map.Update(KeyFrame(Concord::KeyCode::W, true));
    if (!map.IsDown(ActionForward)) return false;

    map.UnbindKey(ActionForward, Concord::KeyCode::W);
    map.BindKey(ActionForward, Concord::KeyCode::Up);
    if (map.IsBound(ActionForward) == false) return false;

    map.Update(KeyFrame(Concord::KeyCode::W, true));
    if (map.IsDown(ActionForward) || !map.WasReleased(ActionForward)) return false;

    map.Update(KeyFrame(Concord::KeyCode::Up, true));
    return map.IsDown(ActionForward) && map.WasPressed(ActionForward);
}

bool TestMouseBinding()
{
    Concord::InputMap map;
    map.BindMouseButton(ActionFire, Concord::MouseButton::Left);
    map.Update(ButtonFrame(Concord::MouseButton::Left, true));
    if (!map.IsDown(ActionFire) || !map.WasPressed(ActionFire)) return false;
    map.Update(ButtonFrame(Concord::MouseButton::Left, false));
    if (map.IsDown(ActionFire) || !map.WasReleased(ActionFire)) return false;

    map.ClearAction(ActionFire);
    map.Update(ButtonFrame(Concord::MouseButton::Left, true));
    return !map.IsBound(ActionFire) && !map.IsDown(ActionFire);
}

bool TestUnboundAndClear()
{
    Concord::InputMap map;
    map.BindKey(ActionJump, Concord::KeyCode::Space);
    map.BindMouseButton(ActionFire, Concord::MouseButton::Right);
    map.Update(KeyFrame(Concord::KeyCode::Space, true));
    if (map.IsDown(99) || map.WasPressed(99) || map.WasReleased(99)) return false;

    map.Clear();
    if (map.IsBound(ActionJump) || map.IsBound(ActionFire)) return false;
    map.Update(KeyFrame(Concord::KeyCode::Space, true));
    return !map.IsDown(ActionJump);
}

bool TestScancodeMapping()
{
    if (Concord::KeyCodeFromSdlScanCode(SDL_SCANCODE_W) != Concord::KeyCode::W) return false;
    if (Concord::KeyCodeFromSdlScanCode(SDL_SCANCODE_SPACE) != Concord::KeyCode::Space) {
        return false;
    }
    if (Concord::KeyCodeFromSdlScanCode(SDL_SCANCODE_LSHIFT) != Concord::KeyCode::LeftShift) {
        return false;
    }
    if (Concord::KeyCodeFromSdlScanCode(SDL_SCANCODE_KP_5) != Concord::KeyCode::KeyPad5) {
        return false;
    }
    if (Concord::KeyCodeFromSdlScanCode(SDL_SCANCODE_F12) != Concord::KeyCode::F12) return false;
    if (Concord::KeyCodeFromSdlScanCode(SDL_SCANCODE_UNKNOWN) != Concord::KeyCode::None) {
        return false;
    }

    Concord::MouseButton button{};
    if (!Concord::MouseButtonFromSdlButton(SDL_BUTTON_LEFT, button) ||
        button != Concord::MouseButton::Left) {
        return false;
    }
    if (!Concord::MouseButtonFromSdlButton(SDL_BUTTON_MIDDLE, button) ||
        button != Concord::MouseButton::Middle) {
        return false;
    }
    return !Concord::MouseButtonFromSdlButton(9, button);
}

} // namespace

int main()
{
    struct Case {
        const char* name;
        bool (*run)();
    };
    const Case cases[] = {
        {"snapshot-trinity", TestSnapshotTrinity},
        {"action-edges", TestActionEdges},
        {"multi-binding-or", TestMultiBindingOr},
        {"runtime-rebinding", TestRuntimeRebinding},
        {"mouse-binding", TestMouseBinding},
        {"unbound-and-clear", TestUnboundAndClear},
        {"scancode-mapping", TestScancodeMapping},
    };
    for (const Case& testCase : cases) {
        if (!testCase.run()) {
            std::printf("FAIL %s\n", testCase.name);
            return 1;
        }
    }
    return 0;
}
