// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderSceneSnapshot.h"

#include "Concord/CCamera.h"
#include "Concord/CScene.h"
#include "engine/render/RenderModelSnapshot.h"
#include "engine/render/RenderSkinningSnapshot.h"

#include <algorithm>
#include <cmath>

namespace Concord {

namespace {

/** Keeps a malformed or minimized viewport from reaching projection math. */
f32 SafeAspect(f32 aspect) noexcept
{
    return std::isfinite(aspect) && aspect > 0.0f ? aspect : 1.0f;
}

/** Clamps malformed transform components before matrix multiplication. */
f32 SafeTransformComponent(f32 value, f32 fallback) noexcept
{
    constexpr f32 kMaxMagnitude = 1000000.0f;
    return std::isfinite(value) ? std::clamp(value, -kMaxMagnitude, kMaxMagnitude) : fallback;
}

/** Replaces malformed vectors while preserving valid authored values. */
Vec3 SafeVector(Vec3 value, Vec3 fallback) noexcept
{
    return {SafeTransformComponent(value.x, fallback.x),
            SafeTransformComponent(value.y, fallback.y),
            SafeTransformComponent(value.z, fallback.z)};
}

/** Keeps primitive extents finite and non-degenerate for every render path. */
Vec3 SafeExtent(Vec3 value) noexcept
{
    const auto KeepExtent = [](f32 component) {
        return std::abs(component) < 0.0001f
                   ? (component < 0.0f ? -0.0001f : 0.0001f)
                   : component;
    };
    const Vec3 safe = SafeVector(value, {1.0f, 1.0f, 1.0f});
    return {KeepExtent(safe.x), KeepExtent(safe.y), KeepExtent(safe.z)};
}

/** Sanitizes a transform and prevents zero extents from reaching AS builds. */
Transform SafeTransform(const Transform& source) noexcept
{
    Transform result = source;
    result.position = SafeVector(source.position, {});
    result.rotation = SafeVector(source.rotation, {});
    result.scale = SafeVector(source.scale, {1.0f, 1.0f, 1.0f});
    result.scale = SafeExtent(result.scale);
    return result;
}

/** Builds a finite model matrix shared by raster, shadow, and ray-query paths. */
Mat4 SafeModel(const Transform& transform, Vec3 size) noexcept
{
    const Transform safeTransform = SafeTransform(transform);
    size = SafeExtent(size);
    const Mat4 model = safeTransform.ToMatrix() * Mat4::Scale(size);
    for (const Vec4& column : model.col) {
        for (u32 i = 0; i < 4; ++i) {
            const f32 component = column[i];
            if (!std::isfinite(component)) {
                return Mat4::Identity();
            }
        }
    }
    return model;
}

} // namespace

RenderSceneSnapshot ExtractRenderScene(const Scene& scene, f32 aspect)
{
    RenderSceneSnapshot snapshot;
    snapshot.environment = scene.Environment();
    const World& world = scene.GetWorld();
    const Entity cameraEntity = scene.MainCamera();
    const CameraComponent* camera = world.Get<CameraComponent>(cameraEntity);
    const Transform* cameraTransform = world.Get<Transform>(cameraEntity);
    if (camera && cameraTransform) {
        snapshot.hasCamera = true;
        snapshot.camera.entity = cameraEntity;
        const Transform safeTransform = SafeTransform(*cameraTransform);
        CameraComponent safeCamera = *camera;
        safeCamera.target = SafeVector(camera->target, {});
        safeCamera.up = SafeVector(camera->up, {0.0f, 1.0f, 0.0f});
        snapshot.camera.position = safeTransform.position;
        snapshot.camera.target = safeCamera.target;
        snapshot.camera.view = Object::ViewMatrix(safeTransform, safeCamera);
        snapshot.camera.projection = Object::ProjectionMatrix(safeCamera, SafeAspect(aspect));
    }

    world.Query<MeshRenderer, Transform>(
        [&](Entity entity, const MeshRenderer& mesh, const Transform& transform) {
            if (!mesh.visible) {
                return;
            }
            Material material{};
            if (const Material* storedMaterial = world.Get<Material>(entity)) {
                material = *storedMaterial;
            }
            const Vec3 safeSize = SafeExtent(mesh.size);
            snapshot.objects.push_back(RenderObjectSnapshot{
                .entity = entity,
                .model = SafeModel(transform, mesh.size),
                .shape = mesh.shape,
                .size = safeSize,
                .material = material,
                .castShadow = mesh.castShadow,
            });
        });

    world.Query<ModelRenderer, Transform>(
        [&](Entity entity, const ModelRenderer& model, const Transform& transform) {
            if (!model.visible || !model.asset || !model.asset->IsValid()) {
                return;
            }
            AppendModelSnapshots(snapshot, entity, model,
                                 SafeModel(transform, {1.0f, 1.0f, 1.0f}));
        });

    AppendSkinningSnapshots(snapshot, world);

    world.Query<LightComponent>([&](Entity entity, const LightComponent& light) {
        RenderLightSnapshot result{};
        result.entity = entity;
        result.light = light;
        if (const Transform* transform = world.Get<Transform>(entity)) {
            result.transform = SafeTransform(*transform);
        }
        snapshot.lights.push_back(result);
    });
    return snapshot;
}

} // namespace Concord
