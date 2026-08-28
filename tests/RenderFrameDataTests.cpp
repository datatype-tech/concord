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
    const Concord::RenderFrameData data = Concord::BuildRenderFrameData(snapshot);
    if (data.header.cameraValid != 1 || data.header.lightCount != 1 ||
        data.header.droppedLightCount != 0 || !Near(data.ambientColorIntensity.w, 0.5f) ||
        !Near(data.lights[0].colorIntensity.w, 2.5f) ||
        !Near(data.lights[0].positionType.w, 0.0f)) {
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
    return bounded.header.lightCount == Concord::kMaxRenderLights &&
                   bounded.header.droppedLightCount == 3
               ? 0
               : 1;
}
