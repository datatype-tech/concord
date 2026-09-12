// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/core/Color.h"
#include "engine/core/Types.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/Components.h"
#include "engine/scene/Camera.h"
#include "engine/scene/ModelRenderer.h"
#include "engine/scene/Scene.h"

#include <cmath>
#include <cstdint>

/**
 * The scene lifecycle, entity edits and environment half of the CVM C ABI.
 *
 * What a script can put into a scene lives in CvmSpawn.cpp; this is what it can
 * then do to it.
 */
namespace {

using Concord::Cvm::Colour;
using Concord::Cvm::EditComponent;
using Concord::Cvm::EditEnvironment;
using Concord::Cvm::EditTransform;
using Concord::Cvm::MeshBounds;
using Concord::Cvm::ToVec3;

} // namespace

extern "C" {

std::int64_t ConcordCvmSceneCreate(void)
{
    return Concord::Cvm::CreateScene();
}

std::int64_t ConcordCvmSceneDestroy(std::int64_t scene)
{
    return Concord::Cvm::DestroyScene(scene);
}

std::int64_t ConcordCvmSceneEntityCount(std::int64_t scene)
{
    const Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    return found == nullptr ? 0 : static_cast<std::int64_t>(found->EntityCount());
}

std::int64_t ConcordCvmSceneHasMainCamera(std::int64_t scene)
{
    const Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    return found->MainCamera().IsValid() ? 1 : 0;
}

std::int64_t ConcordCvmEntitySetPosition(std::int64_t entity, double x, double y, double z)
{
    return EditTransform(entity, [x, y, z](Concord::Transform& transform) {
        transform.position = ToVec3(x, y, z);
    });
}

std::int64_t ConcordCvmEntitySetRotation(std::int64_t entity, double x, double y, double z)
{
    return EditTransform(entity, [x, y, z](Concord::Transform& transform) {
        transform.rotation = ToVec3(x, y, z);
    });
}

std::int64_t ConcordCvmEntityTranslate(std::int64_t entity, double deltaX, double deltaY,
                                       double deltaZ)
{
    return EditTransform(entity, [deltaX, deltaY, deltaZ](Concord::Transform& transform) {
        transform.position.x += static_cast<Concord::f32>(deltaX);
        transform.position.y += static_cast<Concord::f32>(deltaY);
        transform.position.z += static_cast<Concord::f32>(deltaZ);
    });
}

double ConcordCvmEntityPosition(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0.0;
    double result = 0.0;
    EditTransform(entity, [axis, &result](Concord::Transform& transform) {
        const Concord::f32 value = axis == 0   ? transform.position.x
                                   : axis == 1 ? transform.position.y
                                               : transform.position.z;
        result = static_cast<double>(value);
    });
    return result;
}

double ConcordCvmEntityRotation(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0.0;
    double result = 0.0;
    EditTransform(entity, [axis, &result](Concord::Transform& transform) {
        const Concord::f32 value = axis == 0   ? transform.rotation.x
                                   : axis == 1 ? transform.rotation.y
                                               : transform.rotation.z;
        result = static_cast<double>(value);
    });
    return result;
}

std::int64_t ConcordCvmEntitySetScale(std::int64_t entity, double x, double y, double z)
{
    return EditTransform(entity, [x, y, z](Concord::Transform& transform) {
        transform.scale = ToVec3(x, y, z);
    });
}

double ConcordCvmEntityScale(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0.0;
    double result = 0.0;
    EditTransform(entity, [axis, &result](Concord::Transform& transform) {
        const Concord::f32 value = axis == 0   ? transform.scale.x
                                   : axis == 1 ? transform.scale.y
                                               : transform.scale.z;
        result = static_cast<double>(value);
    });
    return result;
}

std::int64_t ConcordCvmEntitySetVisible(std::int64_t entity, std::int64_t visible)
{
    const bool value = visible != 0;
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    if (Concord::MeshRenderer* mesh = scene->GetWorld().Get<Concord::MeshRenderer>(target)) {
        mesh->visible = value;
        return 1;
    }
    if (Concord::ModelRenderer* model = scene->GetWorld().Get<Concord::ModelRenderer>(target)) {
        model->visible = value;
        return 1;
    }
    return 0;
}

std::int64_t ConcordCvmEntitySetShadow(std::int64_t entity, std::int64_t castShadow)
{
    const bool value = castShadow != 0;
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    if (Concord::MeshRenderer* mesh = scene->GetWorld().Get<Concord::MeshRenderer>(target)) {
        mesh->castShadow = value;
        return 1;
    }
    if (Concord::ModelRenderer* model = scene->GetWorld().Get<Concord::ModelRenderer>(target)) {
        model->castShadow = value;
        return 1;
    }
    return 0;
}

std::int64_t ConcordCvmEntitySetColor(std::int64_t entity, std::int64_t red, std::int64_t green,
                                      std::int64_t blue)
{
    return EditComponent<Concord::Material>(entity,
                                            [red, green, blue](Concord::Material& material) {
                                                material.albedo = Colour(red, green, blue);
                                            });
}

std::int64_t ConcordCvmEntitySetMaterial(std::int64_t entity, double metallic, double roughness,
                                         double emissive)
{
    return EditComponent<Concord::Material>(
        entity, [metallic, roughness, emissive](Concord::Material& material) {
            material.metallic = static_cast<Concord::f32>(metallic);
            material.roughness = static_cast<Concord::f32>(roughness);
            material.emissive = static_cast<Concord::f32>(emissive);
        });
}

std::int64_t ConcordCvmEntitySetFov(std::int64_t entity, double fovYDegrees)
{
    return EditComponent<Concord::CameraComponent>(
        entity, [fovYDegrees](Concord::CameraComponent& camera) {
            camera.fovYDegrees = static_cast<Concord::f32>(fovYDegrees);
        });
}

std::int64_t ConcordCvmEntityDestroy(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (Concord::Cvm::ResolveEntity(entity, scene, target)) {
        scene->GetWorld().Destroy(target);
    }
    return Concord::Cvm::DestroyEntity(entity);
}

std::int64_t ConcordCvmSceneSetSky(std::int64_t scene, std::int64_t red, std::int64_t green,
                                   std::int64_t blue)
{
    return EditEnvironment(scene, [red, green, blue](Concord::EnvironmentSettings& settings) {
        settings.skyColor = Colour(red, green, blue);
    });
}

std::int64_t ConcordCvmSceneSetAmbient(std::int64_t scene, std::int64_t red, std::int64_t green,
                                       std::int64_t blue, double intensity)
{
    return EditEnvironment(
        scene, [red, green, blue, intensity](Concord::EnvironmentSettings& settings) {
            settings.ambientColor = Colour(red, green, blue);
            settings.ambientIntensity = static_cast<Concord::f32>(intensity);
        });
}

std::int64_t ConcordCvmSceneSetGrade(std::int64_t scene, double exposure, double contrast,
                                     double saturation, double vignette, double bloomIntensity)
{
    return EditEnvironment(
        scene, [exposure, contrast, saturation, vignette, bloomIntensity](
                   Concord::EnvironmentSettings& settings) {
            settings.exposure = static_cast<Concord::f32>(exposure);
            settings.contrast = static_cast<Concord::f32>(contrast);
            settings.saturation = static_cast<Concord::f32>(saturation);
            settings.vignette = static_cast<Concord::f32>(vignette);
            settings.bloomIntensity = static_cast<Concord::f32>(bloomIntensity);
        });
}

std::int64_t ConcordCvmSceneSetFog(std::int64_t scene, double density, double heightFalloff,
                                   double baseHeight, double anisotropy)
{
    return EditEnvironment(
        scene, [density, heightFalloff, baseHeight, anisotropy](
                   Concord::EnvironmentSettings& settings) {
            settings.fogDensity = static_cast<Concord::f32>(density);
            settings.fogHeightFalloff = static_cast<Concord::f32>(heightFalloff);
            settings.fogBaseHeight = static_cast<Concord::f32>(baseHeight);
            settings.fogAnisotropy = static_cast<Concord::f32>(anisotropy);
        });
}

std::int64_t ConcordCvmSceneSetClouds(std::int64_t scene, double coverage, double density,
                                      double altitude)
{
    return EditEnvironment(scene, [coverage, density, altitude](
                                      Concord::EnvironmentSettings& settings) {
        settings.cloudCoverage = static_cast<Concord::f32>(coverage);
        settings.cloudDensity = static_cast<Concord::f32>(density);
        settings.cloudAltitude = static_cast<Concord::f32>(altitude);
    });
}

std::int64_t ConcordCvmEntityAlive(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    return Concord::Cvm::ResolveEntity(entity, scene, target) ? 1 : 0;
}

std::int64_t ConcordCvmEntityLookAt(std::int64_t entity, double x, double y, double z)
{
    return EditTransform(entity, [x, y, z](Concord::Transform& transform) {
        transform.rotation = Concord::Object::LookAtRotation(transform.position, ToVec3(x, y, z));
    });
}

double ConcordCvmEntityForward(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0.0;
    double result = 0.0;
    EditTransform(entity, [axis, &result](Concord::Transform& transform) {
        const Concord::Vec3 forward = Concord::Object::ForwardVector(transform);
        const Concord::f32 value = axis == 0 ? forward.x : axis == 1 ? forward.y : forward.z;
        result = static_cast<double>(value);
    });
    return result;
}

std::int64_t ConcordCvmEntitySetSize(std::int64_t entity, double x, double y, double z)
{
    return EditComponent<Concord::MeshRenderer>(entity, [x, y, z](Concord::MeshRenderer& mesh) {
        mesh.size = ToVec3(x, y, z);
    });
}

double ConcordCvmEntitySize(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0.0;
    double result = 0.0;
    EditComponent<Concord::MeshRenderer>(entity, [axis, &result](Concord::MeshRenderer& mesh) {
        const Concord::f32 value = axis == 0 ? mesh.size.x : axis == 1 ? mesh.size.y : mesh.size.z;
        result = static_cast<double>(value);
    });
    return result;
}

std::int64_t ConcordCvmEntityOverlaps(std::int64_t left, std::int64_t right)
{
    Concord::Scene* leftScene = nullptr;
    Concord::Scene* rightScene = nullptr;
    Concord::Entity leftEntity;
    Concord::Entity rightEntity;
    if (!Concord::Cvm::ResolveEntity(left, leftScene, leftEntity)) return 0;
    if (!Concord::Cvm::ResolveEntity(right, rightScene, rightEntity)) return 0;
    if (leftScene != rightScene) return 0;
    Concord::Vec3 leftCentre{};
    Concord::Vec3 leftHalf{};
    Concord::Vec3 rightCentre{};
    Concord::Vec3 rightHalf{};
    if (!MeshBounds(*leftScene, leftEntity, leftCentre, leftHalf)) return 0;
    if (!MeshBounds(*rightScene, rightEntity, rightCentre, rightHalf)) return 0;
    if (std::fabs(leftCentre.x - rightCentre.x) > leftHalf.x + rightHalf.x) return 0;
    if (std::fabs(leftCentre.y - rightCentre.y) > leftHalf.y + rightHalf.y) return 0;
    if (std::fabs(leftCentre.z - rightCentre.z) > leftHalf.z + rightHalf.z) return 0;
    return 1;
}

double ConcordCvmEntityDistance(std::int64_t left, std::int64_t right)
{
    Concord::Scene* leftScene = nullptr;
    Concord::Scene* rightScene = nullptr;
    Concord::Entity leftEntity;
    Concord::Entity rightEntity;
    if (!Concord::Cvm::ResolveEntity(left, leftScene, leftEntity)) return 0.0;
    if (!Concord::Cvm::ResolveEntity(right, rightScene, rightEntity)) return 0.0;
    Concord::Transform* leftTransform = leftScene->GetWorld().Get<Concord::Transform>(leftEntity);
    Concord::Transform* rightTransform = rightScene->GetWorld().Get<Concord::Transform>(rightEntity);
    if (leftTransform == nullptr || rightTransform == nullptr) return 0.0;
    const Concord::f32 dx = leftTransform->position.x - rightTransform->position.x;
    const Concord::f32 dy = leftTransform->position.y - rightTransform->position.y;
    const Concord::f32 dz = leftTransform->position.z - rightTransform->position.z;
    return static_cast<double>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

std::int64_t ConcordCvmSceneCamera(std::int64_t scene)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    return Concord::Cvm::FindOrCreateEntity(scene, *found, found->MainCamera());
}

std::int64_t ConcordCvmLightSetIntensity(std::int64_t entity, double intensity)
{
    return EditComponent<Concord::LightComponent>(
        entity, [intensity](Concord::LightComponent& light) {
            light.intensity = static_cast<Concord::f32>(intensity);
        });
}

std::int64_t ConcordCvmLightSetColor(std::int64_t entity, std::int64_t red, std::int64_t green,
                                     std::int64_t blue)
{
    return EditComponent<Concord::LightComponent>(
        entity, [red, green, blue](Concord::LightComponent& light) {
            light.color = Colour(red, green, blue);
        });
}

std::int64_t ConcordCvmLightSetAngles(std::int64_t entity, double elevationDegrees,
                                      double azimuthDegrees)
{
    return EditComponent<Concord::LightComponent>(
        entity, [elevationDegrees, azimuthDegrees](Concord::LightComponent& light) {
            light.elevationDegrees = static_cast<Concord::f32>(elevationDegrees);
            light.azimuthDegrees = static_cast<Concord::f32>(azimuthDegrees);
        });
}

std::int64_t ConcordCvmEntityColor(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0;
    std::int64_t result = 0;
    EditComponent<Concord::Material>(entity, [axis, &result](Concord::Material& material) {
        const Concord::ColorRGBA colour = material.albedo;
        result = axis == 0   ? Concord::ColorR(colour)
                 : axis == 1 ? Concord::ColorG(colour)
                             : Concord::ColorB(colour);
    });
    return result;
}

double ConcordCvmEntityMaterial(std::int64_t entity, std::int64_t field)
{
    if (field < 0 || field > 2) return 0.0;
    double result = 0.0;
    EditComponent<Concord::Material>(entity, [field, &result](Concord::Material& material) {
        const Concord::f32 value = field == 0   ? material.metallic
                                   : field == 1 ? material.roughness
                                                : material.emissive;
        result = static_cast<double>(value);
    });
    return result;
}

double ConcordCvmEntityFov(std::int64_t entity)
{
    double result = 0.0;
    EditComponent<Concord::CameraComponent>(entity, [&result](Concord::CameraComponent& camera) {
        result = static_cast<double>(camera.fovYDegrees);
    });
    return result;
}

std::int64_t ConcordCvmEntityVisible(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    if (Concord::MeshRenderer* mesh = scene->GetWorld().Get<Concord::MeshRenderer>(target)) {
        return mesh->visible ? 1 : 0;
    }
    if (Concord::ModelRenderer* model = scene->GetWorld().Get<Concord::ModelRenderer>(target)) {
        return model->visible ? 1 : 0;
    }
    return 0;
}

std::int64_t ConcordCvmEntityShadow(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    if (Concord::MeshRenderer* mesh = scene->GetWorld().Get<Concord::MeshRenderer>(target)) {
        return mesh->castShadow ? 1 : 0;
    }
    if (Concord::ModelRenderer* model = scene->GetWorld().Get<Concord::ModelRenderer>(target)) {
        return model->castShadow ? 1 : 0;
    }
    return 0;
}

double ConcordCvmLightIntensity(std::int64_t entity)
{
    double result = 0.0;
    EditComponent<Concord::LightComponent>(entity, [&result](Concord::LightComponent& light) {
        result = static_cast<double>(light.intensity);
    });
    return result;
}

std::int64_t ConcordCvmLightColor(std::int64_t entity, std::int64_t axis)
{
    if (axis < 0 || axis > 2) return 0;
    std::int64_t result = 0;
    EditComponent<Concord::LightComponent>(entity, [axis, &result](Concord::LightComponent& light) {
        result = axis == 0   ? Concord::ColorR(light.color)
                 : axis == 1 ? Concord::ColorG(light.color)
                             : Concord::ColorB(light.color);
    });
    return result;
}

double ConcordCvmLightAngle(std::int64_t entity, std::int64_t axis)
{
    if (axis != 0 && axis != 1) return 0.0;
    double result = 0.0;
    EditComponent<Concord::LightComponent>(entity, [axis, &result](Concord::LightComponent& light) {
        result = static_cast<double>(axis == 0 ? light.elevationDegrees : light.azimuthDegrees);
    });
    return result;
}

} // extern "C"
