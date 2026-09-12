// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/core/Types.h"
#include "engine/cvm/CvmModelRegistry.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/ParticleComponents.h"
#include "engine/scene/Box.h"
#include "engine/scene/Camera.h"
#include "engine/scene/EntityHandle.h"
#include "engine/scene/Model.h"
#include "engine/scene/ParticleEmitter.h"
#include "engine/scene/Scene.h"
#include "engine/scene/SunLight.h"
#include "engine/scene/Water.h"
#include "engine/scene/WaterRipple.h"

#include <cstdint>
#include <memory>

/**
 * Everything a script can put into a scene.
 *
 * Split out of CvmScene.cpp once the spawns outnumbered the lifecycle calls:
 * one file that creates things and one that edits them reads better than one
 * that does both.
 */
namespace {

using Concord::Cvm::Colour;
using Concord::Cvm::Record;
using Concord::Cvm::ToVec3;

/** Composes a primitive the engine ships no archetype for. */
std::int64_t SpawnPrimitive(std::int64_t sceneHandle, Concord::PrimitiveShape shape,
                            const Concord::Vec3& position, const Concord::Vec3& size,
                            std::int64_t red, std::int64_t green, std::int64_t blue)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(sceneHandle);
    if (found == nullptr) return 0;
    Concord::EntityHandle handle = found->CreateEntity();
    handle.Add<Concord::Transform>(Concord::Transform{.position = position});
    handle.Add<Concord::MeshRenderer>(Concord::MeshRenderer{.shape = shape, .size = size});
    handle.Add<Concord::Material>(Concord::Material{.albedo = Colour(red, green, blue)});
    return Record(sceneHandle, *found, handle);
}

} // namespace

extern "C" {

std::int64_t ConcordCvmSceneSpawnCamera(std::int64_t scene, double positionX, double positionY,
                                        double positionZ, double targetX, double targetY,
                                        double targetZ)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::Camera>({
        .position = ToVec3(positionX, positionY, positionZ),
        .target = ToVec3(targetX, targetY, targetZ),
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnSun(std::int64_t scene, double elevationDegrees,
                                     double azimuthDegrees, double intensity)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::SunLight>({
        .elevationDegrees = static_cast<Concord::f32>(elevationDegrees),
        .azimuthDegrees = static_cast<Concord::f32>(azimuthDegrees),
        .intensity = static_cast<Concord::f32>(intensity),
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnBox(std::int64_t scene, double positionX, double positionY,
                                     double positionZ, double sizeX, double sizeY, double sizeZ,
                                     std::int64_t red, std::int64_t green, std::int64_t blue)
{
    return SpawnPrimitive(scene, Concord::PrimitiveShape::Box,
                          ToVec3(positionX, positionY, positionZ), ToVec3(sizeX, sizeY, sizeZ),
                          red, green, blue);
}

std::int64_t ConcordCvmSceneSpawnSphere(std::int64_t scene, double positionX, double positionY,
                                        double positionZ, double diameter, std::int64_t red,
                                        std::int64_t green, std::int64_t blue)
{
    const Concord::f32 extent = static_cast<Concord::f32>(diameter);
    return SpawnPrimitive(scene, Concord::PrimitiveShape::Sphere,
                          ToVec3(positionX, positionY, positionZ), {extent, extent, extent}, red,
                          green, blue);
}

std::int64_t ConcordCvmSceneSpawnPlane(std::int64_t scene, double positionX, double positionY,
                                       double positionZ, double sizeX, double sizeZ,
                                       std::int64_t red, std::int64_t green, std::int64_t blue)
{
    // A plane has no thickness; the middle extent is left at one so the shape
    // is a filled quad rather than something degenerate.
    return SpawnPrimitive(scene, Concord::PrimitiveShape::Plane,
                          ToVec3(positionX, positionY, positionZ), ToVec3(sizeX, 1.0, sizeZ), red,
                          green, blue);
}

std::int64_t ConcordCvmSceneSpawnPointLight(std::int64_t scene, double positionX, double positionY,
                                            double positionZ, std::int64_t red, std::int64_t green,
                                            std::int64_t blue, double intensity, double range)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    Concord::EntityHandle handle = found->CreateEntity();
    handle.Add<Concord::Transform>(
        Concord::Transform{.position = ToVec3(positionX, positionY, positionZ)});
    handle.Add<Concord::LightComponent>(Concord::LightComponent{
        .type = Concord::LightType::Point,
        .color = Colour(red, green, blue),
        .intensity = static_cast<Concord::f32>(intensity),
        .range = static_cast<Concord::f32>(range),
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnSpotLight(std::int64_t scene, double positionX, double positionY,
                                           double positionZ, double elevationDegrees,
                                           double azimuthDegrees, std::int64_t red,
                                           std::int64_t green, std::int64_t blue, double intensity,
                                           double range, double angleDegrees)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    Concord::EntityHandle handle = found->CreateEntity();
    handle.Add<Concord::Transform>(
        Concord::Transform{.position = ToVec3(positionX, positionY, positionZ)});
    handle.Add<Concord::LightComponent>(Concord::LightComponent{
        .type = Concord::LightType::Spot,
        .color = Colour(red, green, blue),
        .intensity = static_cast<Concord::f32>(intensity),
        .elevationDegrees = static_cast<Concord::f32>(elevationDegrees),
        .azimuthDegrees = static_cast<Concord::f32>(azimuthDegrees),
        .range = static_cast<Concord::f32>(range),
        .spotAngleDegrees = static_cast<Concord::f32>(angleDegrees),
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnModel(std::int64_t scene, std::int64_t model, double positionX,
                                       double positionY, double positionZ)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    if (asset == nullptr) return 0;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::Model>({
        .asset = std::move(asset),
        .transform = {.position = ToVec3(positionX, positionY, positionZ)},
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnParticles(std::int64_t scene, double positionX, double positionY,
                                           double positionZ, std::int64_t red, std::int64_t green,
                                           std::int64_t blue)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::ParticleEmitter>({
        .transform = {.position = ToVec3(positionX, positionY, positionZ)},
        .settings = {.startColor = Colour(red, green, blue)},
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnWater(std::int64_t scene, std::int64_t model, double positionX,
                                       double positionY, double positionZ, double scale,
                                       std::int64_t red, std::int64_t green, std::int64_t blue,
                                       double opacity, double absorptionDistance,
                                       double waveAmplitude, double waveSpeed)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    if (asset == nullptr) return 0;

    Concord::WaterMaterial surface;
    // The surface colour is what light is left with after the medium has taken
    // its share, so the authored colour is the shallow-water tint rather than
    // the absorption curve; absorptionDistance is what turns it into depth.
    surface.absorption = ToVec3(static_cast<double>(255 - red) / 255.0,
                                static_cast<double>(255 - green) / 255.0,
                                static_cast<double>(255 - blue) / 255.0);
    surface.opacity = static_cast<Concord::f32>(opacity);
    surface.absorptionDistance = static_cast<Concord::f32>(absorptionDistance);
    surface.waveAmplitude = static_cast<Concord::f32>(waveAmplitude);
    surface.waveSpeed = static_cast<Concord::f32>(waveSpeed);

    const Concord::f32 extent = static_cast<Concord::f32>(scale);
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::Water>({
        .asset = std::move(asset),
        .surface = surface,
        .transform = {.position = ToVec3(positionX, positionY, positionZ),
                      .scale = {extent, 1.0f, extent}},
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnRipple(std::int64_t scene, double x, double z, double wavelength,
                                        double speed, double strength, double reach,
                                        double lifetime)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::WaterRipple>({
        .ripple =
            {
                .centre = {static_cast<Concord::f32>(x), static_cast<Concord::f32>(z)},
                .wavelength = static_cast<Concord::f32>(wavelength),
                .speed = static_cast<Concord::f32>(speed),
                .strength = static_cast<Concord::f32>(strength),
                .reach = static_cast<Concord::f32>(reach),
            },
        .lifetime = static_cast<Concord::f32>(lifetime),
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnRippleField(std::int64_t scene, double rate, double extentX,
                                             double extentZ)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    Concord::WaterRippleFieldSettings settings;
    settings.rate = static_cast<Concord::f32>(rate);
    settings.extent = {static_cast<Concord::f32>(extentX), static_cast<Concord::f32>(extentZ)};
    const Concord::EntityHandle handle =
        found->Spawn<Concord::Object::WaterRippleField>({.settings = settings});
    return Record(scene, *found, handle);
}

} // extern "C"
