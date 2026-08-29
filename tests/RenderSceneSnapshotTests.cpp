// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderSceneSnapshot.h"

#include "engine/scene/Scene.h"

#include <cmath>
#include <limits>
#include <memory>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.0001f;
}

} // namespace

int main()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({.position = {0.0f, 0.0f, -5.0f}});
    scene.Spawn<Concord::Object::Camera>({.priority = 10});
    const auto preferredCamera = scene.Spawn<Concord::Object::Camera>({.priority = -2});
    scene.Spawn<Concord::Object::Box>({
        .transform = {.position = {2.0f, 1.0f, 0.0f}},
        .size = {2.0f, 3.0f, 4.0f},
        .material = {.metallic = 0.6f, .roughness = 0.25f, .emissive = 0.15f},
    });
    scene.Spawn<Concord::Object::Box>({.visible = false});
    scene.Spawn<Concord::Object::SunLight>({});
    scene.SetEnvironment({
        .skyColor = COLOR_RGB(12, 24, 36),
        .ambientColor = COLOR_RGB(80, 100, 120),
        .ambientIntensity = 0.75f,
    });

    const Concord::RenderSceneSnapshot snapshot =
        Concord::ExtractRenderScene(scene, 16.0f / 9.0f);
    if (!snapshot.hasCamera || snapshot.camera.entity != preferredCamera.Id() ||
        snapshot.objects.size() != 1 || snapshot.lights.size() != 1) {
        return 1;
    }
    if (!Near(snapshot.objects[0].model.col[3].x, 2.0f) ||
        !Near(snapshot.objects[0].model.col[3].y, 1.0f) ||
        !Near(snapshot.objects[0].model.col[0].x, 2.0f) ||
        !Near(snapshot.objects[0].model.col[1].y, 3.0f) ||
        !Near(snapshot.objects[0].model.col[2].z, 4.0f) ||
        !Near(snapshot.objects[0].size.z, 4.0f) ||
        snapshot.objects[0].shape != Concord::PrimitiveShape::Box ||
        Concord::ColorR(snapshot.objects[0].material.albedo) != 200) {
        return 1;
    }
    if (!Near(snapshot.objects[0].material.metallic, 0.6f) ||
        !Near(snapshot.objects[0].material.roughness, 0.25f) ||
        !Near(snapshot.objects[0].material.emissive, 0.15f)) {
        return 1;
    }
    if (Concord::ColorR(snapshot.environment.skyColor) != 12 ||
        Concord::ColorG(snapshot.environment.ambientColor) != 100 ||
        !Near(snapshot.environment.ambientIntensity, 0.75f)) {
        return 1;
    }

    auto imported = std::make_shared<Concord::ModelAsset>();
    imported->materials.push_back(Concord::ModelMaterial{});
    Concord::ModelPrimitive triangle{};
    triangle.vertices = {
        Concord::ModelVertex{.position = {-1.0f, 0.0f, 0.0f}},
        Concord::ModelVertex{.position = {1.0f, 0.0f, 0.0f}},
        Concord::ModelVertex{.position = {0.0f, 1.0f, 0.0f}},
    };
    triangle.indices = {0, 1, 2};
    imported->meshes.push_back(Concord::ModelMesh{.primitives = {triangle}});
    imported->nodes.push_back(Concord::ModelNode{
        .name = "triangle", .local = {.translation = {2.0f, 0.0f, 0.0f}}, .mesh = 0});
    Concord::Scene importedScene;
    importedScene.Spawn<Concord::Object::Camera>({});
    importedScene.Spawn<Concord::Object::Model>({
        .asset = imported, .transform = {.position = {3.0f, 0.0f, 0.0f}},
    });
    const Concord::RenderSceneSnapshot importedSnapshot =
        Concord::ExtractRenderScene(importedScene, 1.0f);
    if (importedSnapshot.objects.size() != 1 ||
        importedSnapshot.objects[0].shape != Concord::PrimitiveShape::Model ||
        importedSnapshot.objects[0].modelAsset.get() != imported.get() ||
        !Near(importedSnapshot.objects[0].model.col[3].x, 5.0f) ||
        importedSnapshot.objects[0].modelNode != 0) {
        return 1;
    }
    if (!Near(snapshot.camera.projection.col[0].x, 0.97427857f)) {
        return 1;
    }

    Concord::Scene malformed;
    malformed.Spawn<Concord::Object::Camera>({
        .fovYDegrees = std::numeric_limits<float>::quiet_NaN(),
        .nearPlane = -1.0f,
        .farPlane = 0.01f,
    });
    const Concord::RenderSceneSnapshot safe = Concord::ExtractRenderScene(malformed, 0.0f);
    if (!safe.hasCamera || !std::isfinite(safe.camera.projection.col[0].x) ||
        !std::isfinite(safe.camera.projection.col[2].z)) {
        return 1;
    }

    Concord::Scene malformedObject;
    malformedObject.Spawn<Concord::Object::Box>({
        .transform = {.position = {std::numeric_limits<float>::quiet_NaN(), 1.0f, 2.0f},
                      .scale = {0.0f, std::numeric_limits<float>::infinity(), 1.0f}},
        .size = {std::numeric_limits<float>::quiet_NaN(), 2.0f, 3.0f},
    });
    const Concord::RenderSceneSnapshot safeObject =
        Concord::ExtractRenderScene(malformedObject, 1.0f);
    if (safeObject.objects.size() != 1 ||
        !std::isfinite(safeObject.objects[0].model.col[3].x) ||
        !std::isfinite(safeObject.objects[0].model.col[1].y) ||
        safeObject.objects[0].size.x <= 0.0f) {
        return 1;
    }

    Concord::Scene noCamera;
    const Concord::RenderSceneSnapshot empty = Concord::ExtractRenderScene(noCamera, 0.0f);
    return empty.hasCamera || !empty.objects.empty() || !empty.lights.empty() ? 1 : 0;
}
