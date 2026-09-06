// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_FIRSTPERSONCONTROLLER_H
#define CONCORD_FIRSTPERSONCONTROLLER_H

#include "Concord/CExport.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/System.h"
#include "engine/input/InputMap.h"
#include "engine/window/Window.h"

namespace Concord {

/** Actions the first-person controller drives through its InputMap. */
enum class ControllerAction : u32 {
    Forward,
    Backward,
    Left,
    Right,
    FlyUp,
    FlyDown,
    Sprint,
};

class CENGINE_API FirstPersonController final : public ISystem {
public:
    struct Settings {
        /** Horizontal movement speed in world units per second. */
        f32 moveSpeed = 8.0f;
        /** Mouse motion to degrees of yaw/pitch. */
        f32 mouseSensitivity = 0.12f;
        /** Absolute pitch limit in degrees. */
        f32 maxPitch = 89.0f;
        /** Whether vertical movement is enabled through the fly bindings. */
        bool flyMode = false;
        /** Speed multiplier while the sprint action is held. */
        f32 sprintMultiplier = 1.6f;
        /** Key bindings applied to the action map at construction. */
        KeyCode forward = KeyCode::W;
        KeyCode backward = KeyCode::S;
        KeyCode left = KeyCode::A;
        KeyCode right = KeyCode::D;
        KeyCode flyUp = KeyCode::Space;
        KeyCode flyDown = KeyCode::LeftControl;
        KeyCode sprint = KeyCode::LeftShift;
        /** Mouse button that captures the cursor for mouse look. */
        MouseButton captureButton = MouseButton::Left;
    };

    explicit FirstPersonController(Window& window) noexcept;
    FirstPersonController(Window& window, Settings settings) noexcept;

    void OnUpdate(Scene& scene, f32 deltaTime) override;

    /** Live action map; rebind actions between frames for runtime controls. */
    [[nodiscard]] InputMap& Actions() noexcept { return m_actions; }

private:
    Window& m_window;
    Settings m_settings;
    InputMap m_actions;
    Entity m_camera{};
    f32 m_yaw = 0.0f;
    f32 m_pitch = 0.0f;
    bool m_anglesInitialized = false;
};

} // namespace Concord

#endif // CONCORD_FIRSTPERSONCONTROLLER_H
