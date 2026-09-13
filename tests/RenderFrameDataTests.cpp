// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderFrameData.h"

#include "engine/scene/Scene.h"

#include <cmath>
#include <cstddef>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.0001f;
}

} // namespace

int main()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({});
    scene.Spawn<Concord::Object::SunLight>({.intensity = 2.5f});
    Concord::RenderSceneSnapshot snapshot =
        Concord::ExtractRenderScene(scene, 16.0f / 9.0f);
    snapshot.environment.ambientColor = COLOR_RGB(123, 148, 170);
    snapshot.environment.ambientIntensity = 0.5f;
    snapshot.environment.exposure = 2.5f;
    snapshot.environment.contrast = 0.4f;
    snapshot.environment.saturation = 1.2f;
    snapshot.environment.vignette = 0.3f;
    snapshot.environment.bloomThreshold = 0.8f;
    snapshot.environment.bloomIntensity = 0.5f;
    snapshot.environment.bloomRadius = 24.0f;
    snapshot.environment.chromaticAberration = 0.002f;
    snapshot.environment.fogDensity = 0.05f;
    snapshot.environment.fogHeightFalloff = 0.08f;
    snapshot.environment.fogBaseHeight = 1.5f;
    snapshot.environment.fogAnisotropy = 0.6f;
    snapshot.environment.fogSunScattering = 1.2f;
    snapshot.environment.fogAmbientScattering = 0.4f;
    snapshot.environment.fogSteps = 6;
    snapshot.environment.fogStepDistance = 30.0f;
    const Concord::RenderFrameData data = Concord::BuildRenderFrameData(snapshot);
    if (data.header.cameraValid != 1 || data.header.lightCount != 1 ||
        data.header.droppedLightCount != 0 || !Near(data.ambientColorIntensity.w, 0.5f) ||
        !Near(data.lights[0].colorIntensity.w, 2.5f) ||
        !Near(data.lights[0].positionType.w, 0.0f)) {
        return 1;
    }
    // The look rides in the frame block, so it has to survive the packing
    // exactly: an exposure that arrives as zero blanks the frame, and a
    // vignette outside 0..1 brightens the corners instead of darkening them.
    if (!Near(data.grade.x, 2.5f) || !Near(data.grade.y, 0.4f) ||
        !Near(data.grade.z, 1.2f) || !Near(data.grade.w, 0.3f) ||
        !Near(data.postFx.x, 0.8f) || !Near(data.postFx.y, 0.5f) ||
        !Near(data.postFx.z, 24.0f) || !Near(data.postFx.w, 0.002f)) {
        return 1;
    }
    if (!Near(data.fog.x, 0.05f) || !Near(data.fog.y, 0.08f) || !Near(data.fog.z, 1.5f) ||
        !Near(data.fog.w, 0.6f) || !Near(data.fogLight.x, 1.2f) ||
        !Near(data.fogLight.y, 0.4f) || !Near(data.fogLight.z, 6.0f) ||
        !Near(data.fogLight.w, 30.0f)) {
        return 1;
    }
    Concord::RenderSceneSnapshot hostile = snapshot;
    hostile.environment.exposure = -3.0f;
    hostile.environment.contrast = 9.0f;
    hostile.environment.chromaticAberration = 1.0f;
    hostile.environment.bloomRadius = 9000.0f;
    // A negative height falloff would thicken the medium without limit as it
    // rises, and an anisotropy past 1 has no phase function at all.
    hostile.environment.fogHeightFalloff = -4.0f;
    hostile.environment.fogAnisotropy = 40.0f;
    hostile.environment.fogSteps = 9999u;
    const Concord::RenderFrameData clamped = Concord::BuildRenderFrameData(hostile);
    if (clamped.grade.x != 0.0f || !Near(clamped.grade.y, 1.0f) ||
        !Near(clamped.postFx.w, 0.05f) || !Near(clamped.postFx.z, 512.0f)) {
        return 1;
    }
    // Anisotropy is stored biased by one so the whole -1..1 range fits the
    // unsigned clamp, so the saturated value here is one and not 0.95.
    if (clamped.fog.y != 0.0f || !Near(clamped.fog.w, 1.0f) ||
        !Near(clamped.fogLight.z, 8.0f)) {
        return 1;
    }
    const std::span<const std::byte> bytes = Concord::RenderFrameDataBytes(data);
    if (bytes.size() != sizeof(Concord::RenderFrameData) ||
        bytes.data() != reinterpret_cast<const std::byte*>(&data)) {
        return 1;
    }

    Concord::RenderSceneSnapshot many = snapshot;
    many.lights.resize(Concord::kMaxRenderLights + 3);
    for (Concord::RenderLightSnapshot& light : many.lights) {
        light.light.type = Concord::LightType::Point;
    }
    const Concord::RenderFrameData bounded = Concord::BuildRenderFrameData(many);
    if (bounded.header.lightCount != Concord::kMaxRenderLights ||
        bounded.header.droppedLightCount != 3) {
        return 1;
    }

    // Water bodies pack as xy centre, z world height, w radius, and an unused
    // tail slot stays a zero radius -- the empty-slot sentinel the shader's
    // own loop bails out on.
    Concord::RenderSceneSnapshot wet = snapshot;
    wet.waterBodies.push_back(
        Concord::RenderWaterBodySnapshot{.centre = {2.0f, -3.0f}, .worldY = 0.5f, .radius = 6.0f});
    const Concord::RenderFrameData wetData = Concord::BuildRenderFrameData(wet);
    if (!Near(wetData.wetnessBodies[0].x, 2.0f) || !Near(wetData.wetnessBodies[0].y, 0.5f) ||
        !Near(wetData.wetnessBodies[0].z, -3.0f) || !Near(wetData.wetnessBodies[0].w, 6.0f) ||
        wetData.wetnessBodies[1].w != 0.0f) {
        return 1;
    }

    // The wetness list is bounded the same way the light list is: anything
    // past capacity is dropped rather than grown, since the shader's own loop
    // is sized to the constant.
    Concord::RenderSceneSnapshot manyWet = snapshot;
    for (Concord::u32 index = 0; index < Concord::kMaxWetnessBodies + 4u; ++index) {
        manyWet.waterBodies.push_back(Concord::RenderWaterBodySnapshot{.radius = 1.0f});
    }
    const Concord::RenderFrameData boundedWet = Concord::BuildRenderFrameData(manyWet);
    for (Concord::u32 index = 0; index < Concord::kMaxWetnessBodies; ++index) {
        if (boundedWet.wetnessBodies[index].w != 1.0f) {
            return 1;
        }
    }
    return 0;
}
