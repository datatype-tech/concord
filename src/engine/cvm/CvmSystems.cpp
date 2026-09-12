// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmSystems.h"

#include "engine/app/Game.h"
#include "engine/core/Types.h"
#include "engine/cvm/CvmApi.h"
#include "engine/ecs/AnimationSystem.h"
#include "engine/ecs/DayCycleSystem.h"
#include "engine/ecs/ParticleSystem.h"
#include "engine/ecs/PhysicsSystem.h"
#include "engine/ecs/WaterRippleSystem.h"
#include "engine/scene/DayCycle.h"
#include "engine/scene/FirstPersonController.h"
#include "engine/window/Window.h"

#include <cstdint>
#include <mutex>

/**
 * The engine systems a script asked for.
 *
 * The entry points are defined at global scope rather than inside
 * Concord::Cvm: an extern "C" declaration in a namespace is a different
 * declaration from the global one in CvmApi.h, and the definition would not
 * inherit that declaration's dllexport -- which shows up as symbols missing
 * from the DLL rather than as a compile error.
 */
namespace Concord::Cvm {

namespace {

std::mutex g_systemsMutex;

/** What a script asked for. */
struct Requests {
    bool particles = false;
    bool ripples = false;
    bool animation = false;
    bool dayCycle = false;
    DayCycleSettings dayCycleSettings{};
    bool firstPerson = false;
    bool flyMode = false;
    bool physics = false;
};

Requests g_requests;
FirstPersonController* g_firstPerson = nullptr;

} // namespace

void ApplyRequestedSystems(Game& game, Window& window)
{
    Requests requests;
    {
        std::lock_guard<std::mutex> guard(g_systemsMutex);
        requests = g_requests;
    }

    if (requests.animation) game.Systems().Add<AnimationSystem>();
    if (requests.particles) game.Systems().Add<ParticleSystem>();
    if (requests.physics) game.Systems().Add<PhysicsSystem>();
    if (requests.ripples) game.Systems().Add<WaterRippleSystem>();
    if (requests.dayCycle) game.Systems().Add<DayCycleSystem>(requests.dayCycleSettings);
    if (requests.firstPerson) {
        FirstPersonController& controller = game.Systems().Add<FirstPersonController>(
            window, FirstPersonController::Settings{.flyMode = requests.flyMode});
        std::lock_guard<std::mutex> guard(g_systemsMutex);
        g_firstPerson = &controller;
    } else {
        std::lock_guard<std::mutex> guard(g_systemsMutex);
        g_firstPerson = nullptr;
    }
}

void ClearLiveSystems()
{
    std::lock_guard<std::mutex> guard(g_systemsMutex);
    g_firstPerson = nullptr;
}

} // namespace Concord::Cvm

extern "C" {

CENGINE_API std::int64_t ConcordCvmAddParticleSystem(void)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.particles = true;
    return 1;
}

CENGINE_API std::int64_t ConcordCvmAddWaterRippleSystem(void)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.ripples = true;
    return 1;
}

CENGINE_API std::int64_t ConcordCvmAddAnimationSystem(void)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.animation = true;
    return 1;
}

CENGINE_API std::int64_t ConcordCvmAddDayCycle(double secondsPerDay, double startHour,
                                               double peakSunElevationDegrees,
                                               double sunriseAzimuthDegrees, double cloudCoverage)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.dayCycle = true;
    Concord::Cvm::g_requests.dayCycleSettings = Concord::DayCycleSettings{
        .secondsPerDay = static_cast<Concord::f32>(secondsPerDay),
        .startHour = static_cast<Concord::f32>(startHour),
        .peakSunElevationDegrees = static_cast<Concord::f32>(peakSunElevationDegrees),
        .sunriseAzimuthDegrees = static_cast<Concord::f32>(sunriseAzimuthDegrees),
        .cloudCoverage = static_cast<Concord::f32>(cloudCoverage),
    };
    return 1;
}

CENGINE_API std::int64_t ConcordCvmAddFirstPerson(std::int64_t flyMode)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.firstPerson = true;
    Concord::Cvm::g_requests.flyMode = flyMode != 0;
    return 1;
}

CENGINE_API std::int64_t ConcordCvmAddPhysicsSystem(void)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.physics = true;
    return 1;
}

CENGINE_API std::int64_t ConcordCvmSetFlyMode(std::int64_t flyMode)
{
    std::lock_guard<std::mutex> guard(Concord::Cvm::g_systemsMutex);
    Concord::Cvm::g_requests.flyMode = flyMode != 0;
    if (Concord::Cvm::g_firstPerson != nullptr) {
        Concord::Cvm::g_firstPerson->SetFlyMode(flyMode != 0);
    }
    return 1;
}

} // extern "C"
