// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/scene/FirstPersonController.h"

#include "engine/ecs/Components.h"
#include "engine/scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Concord {

namespace {

constexpr f32 kFullTurnDegrees = 360.0f;

/** Keeps an accumulated yaw bounded without changing its direction. */
f32 WrapDegrees(f32 degrees) noexcept
{
    if (!std::isfinite(degrees)) {
        return 0.0f;
    }
    degrees = std::fmod(degrees + 180.0f, kFullTurnDegrees);
    if (degrees < 0.0f) {
        degrees += kFullTurnDegrees;
    }
    return degrees - 180.0f;
}

/** Returns a normalized movement vector when input is non-zero. */
Vec3 NormalizeMovement(Vec3 movement) noexcept
{
    const f32 lengthSquared = Dot(movement, movement);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f) {
        return {};
    }
    return movement / std::sqrt(lengthSquared);
}

} // namespace

FirstPersonController::FirstPersonController(Window& window) noexcept
    : FirstPersonController(window, Settings{})
{
}

FirstPersonController::FirstPersonController(Window& window, Settings settings) noexcept
    : m_window(window), m_settings(settings)
{
    m_settings.moveSpeed = std::isfinite(m_settings.moveSpeed) && m_settings.moveSpeed > 0.0f
                               ? std::clamp(m_settings.moveSpeed, 0.01f, 10000.0f)
                               : 8.0f;
    m_settings.mouseSensitivity = std::isfinite(m_settings.mouseSensitivity) &&
                                          m_settings.mouseSensitivity > 0.0f
                                      ? std::clamp(m_settings.mouseSensitivity, 0.001f, 100.0f)
                                      : 0.12f;
    m_settings.maxPitch = std::isfinite(m_settings.maxPitch)
                              ? std::clamp(m_settings.maxPitch, 1.0f, 89.0f)
                              : 89.0f;
    m_settings.sprintMultiplier = std::isfinite(m_settings.sprintMultiplier) &&
                                          m_settings.sprintMultiplier > 0.0f
                                      ? std::clamp(m_settings.sprintMultiplier, 0.1f, 16.0f)
                                      : 1.6f;
}

void FirstPersonController::OnUpdate(Scene& scene, f32 deltaTime)
{
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        return;
    }
    if (m_window.WasMouseButtonPressed(MouseButton::Left)) {
        m_window.SetMouseCaptured(true);
    }

    const Entity cameraEntity = scene.MainCamera();
    if (cameraEntity != m_camera) {
        m_camera = cameraEntity;
        m_anglesInitialized = false;
    }
    if (!m_camera.IsValid()) {
        return;
    }

    scene.Query<Transform, CameraComponent>(
        [&](Entity entity, Transform& transform, CameraComponent&) {
            if (entity != m_camera) {
                return;
            }
            if (!m_anglesInitialized) {
                m_yaw = std::isfinite(transform.rotation.y) ? WrapDegrees(transform.rotation.y) : 0.0f;
                m_pitch = std::isfinite(transform.rotation.x) ? transform.rotation.x : 0.0f;
                m_pitch = std::clamp(m_pitch, -m_settings.maxPitch, m_settings.maxPitch);
                m_anglesInitialized = true;
            }

            if (m_window.IsMouseCaptured()) {
                const Vec2 mouse = m_window.MouseDelta();
                if (std::isfinite(mouse.x)) {
                    m_yaw = WrapDegrees(m_yaw - mouse.x * m_settings.mouseSensitivity);
                }
                if (std::isfinite(mouse.y)) {
                    m_pitch -= mouse.y * m_settings.mouseSensitivity;
                    m_pitch = std::clamp(m_pitch, -m_settings.maxPitch, m_settings.maxPitch);
                }
            }
            transform.rotation.x = m_pitch;
            transform.rotation.y = m_yaw;

            const f32 yaw = Radians(m_yaw);
            const Vec3 forward{-std::sin(yaw), 0.0f, -std::cos(yaw)};
            const Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
            Vec3 movement{};
            if (m_window.IsKeyDown(Key::W)) movement += forward;
            if (m_window.IsKeyDown(Key::S)) movement -= forward;
            if (m_window.IsKeyDown(Key::D)) movement += right;
            if (m_window.IsKeyDown(Key::A)) movement -= right;
            if (m_settings.flyMode) {
                if (m_window.IsKeyDown(Key::Space)) movement.y += 1.0f;
                if (m_window.IsKeyDown(Key::Control)) movement.y -= 1.0f;
            }

            const Vec3 direction = NormalizeMovement(movement);
            if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) {
                return;
            }
            f32 speed = m_settings.moveSpeed;
            if (m_window.IsKeyDown(Key::Shift)) {
                speed *= m_settings.sprintMultiplier;
            }
            transform.position += direction * (speed * deltaTime);
        });
}

} // namespace Concord
