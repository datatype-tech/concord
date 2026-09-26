// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include <algorithm>

namespace Concord::Editor {
void Workspace::SceneSettings()
{
    const SceneDocument before=m_document;
    bool changed=false;
    const auto number=[&](const char* label,float& value,float step,float low,float high) {
        ImGui::TextDisabled("%s",label);ImGui::PushID(label);
        changed|=ImGui::DragFloat("##value",&value,step,low,high,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        RecordEdit();ImGui::PopID();
    };
    const auto color=[&](const char* label,ColorRGBA& value) {
        float channels[]={ColorR(value)/255.0f,ColorG(value)/255.0f,ColorB(value)/255.0f};
        const bool edit=ImGui::ColorEdit3(label,channels,ImGuiColorEditFlags_NoInputs);RecordEdit();
        if(edit){value=MakeColor(static_cast<u8>(channels[0]*255),static_cast<u8>(channels[1]*255),static_cast<u8>(channels[2]*255));changed=true;}
    };
    ImGui::TextWrapped("%s",Utf8Text(m_scenePath.lexically_relative(m_project)).c_str());
    if(Design::Action("Make initial scene","play","Set as initial scene"))Attempt([&]{auto settings=m_projectConfig;settings.startupScene=Utf8Text(m_scenePath.lexically_relative(m_project));ApplyProjectSettings(settings);m_status="Initial scene saved";});
    ImGui::PushItemWidth(-1);
    if(ImGui::CollapsingHeader("Game camera",ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& camera=m_document.camera;
        if(ImGui::Button("Use current 3D view")){Checkpoint();camera.position=m_eye;camera.target=m_target;changed=true;}
        ImGui::TextDisabled("Position");changed|=ImGui::DragFloat3("##gameCameraPosition",&camera.position.x,0.1f,-10000,10000);RecordEdit();
        ImGui::TextDisabled("Look at");changed|=ImGui::DragFloat3("##gameCameraTarget",&camera.target.x,0.1f,-10000,10000);RecordEdit();
        bool orthographic=camera.orthographic;
        if(ImGui::Checkbox("Orthographic",&orthographic)){Checkpoint();camera.orthographic=orthographic;changed=true;}
        number("Field of view",camera.fovYDegrees,0.5f,5,160);
        if(camera.orthographic)number("Orthographic size",camera.orthographicSize,0.1f,0.01f,10000);
        number("Near plane",camera.nearPlane,0.01f,0.001f,camera.farPlane-0.01f);
        number("Far plane",camera.farPlane,1,camera.nearPlane+0.01f,100000);
    }
    if(ImGui::CollapsingHeader("Sunlight",ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& sun=m_document.sun;
        number("Elevation",sun.elevationDegrees,0.5f,-90,90);
        number("Azimuth",sun.azimuthDegrees,0.5f,-360,360);
        number("Intensity",sun.intensity,0.05f,0,100);color("Sun color",sun.color);
        bool shadow=sun.castShadow;
        if(ImGui::Checkbox("Cast shadows",&shadow)){Checkpoint();sun.castShadow=shadow;changed=true;}
    }
    auto& environment=m_document.environment;
    if(ImGui::CollapsingHeader("Sky and ambient")) {
        changed|=ImGui::ColorEdit3("Zenith",&environment.zenithColor.x);RecordEdit();
        changed|=ImGui::ColorEdit3("Horizon",&environment.horizonColor.x);RecordEdit();
        color("Clear color",environment.skyColor);color("Ambient color",environment.ambientColor);
        number("Ambient intensity",environment.ambientIntensity,0.01f,0,10);
        number("Sun multiplier",environment.sunIntensity,0.01f,0,20);
        number("Moon intensity",environment.moonIntensity,0.01f,0,20);
        ImGui::TextDisabled("Moon direction");changed|=ImGui::DragFloat3("##moonDirection",&environment.moonDirection.x,0.01f,-1,1);RecordEdit();
    }
    if(ImGui::CollapsingHeader("Color and post processing")) {
        number("Exposure",environment.exposure,0.01f,0.01f,20);
        number("Contrast",environment.contrast,0.01f,0,2);
        number("Saturation",environment.saturation,0.01f,0,3);
        number("Vignette",environment.vignette,0.01f,0,1);
        number("Bloom threshold",environment.bloomThreshold,0.1f,0,100);
        number("Bloom intensity",environment.bloomIntensity,0.01f,0,10);
        number("Bloom radius",environment.bloomRadius,0.5f,0,100);
        number("Chromatic aberration",environment.chromaticAberration,0.001f,0,0.1f);
    }
    if(ImGui::CollapsingHeader("Volumetric clouds")) {
        number("Coverage",environment.cloudCoverage,0.01f,0,1);
        number("Density",environment.cloudDensity,0.0001f,0,1);
        number("Altitude",environment.cloudAltitude,1,-10000,10000);
        number("Thickness",environment.cloudThickness,1,1,10000);
        number("Weather scale",environment.cloudWeatherScale,1,1,10000);
        number("Detail scale",environment.cloudDetailScale,1,1,10000);
        number("Detail strength",environment.cloudDetailStrength,0.01f,0,1);
        number("Cloud type",environment.cloudType,0.01f,0,1);
        number("Drift speed",environment.cloudDriftSpeed,0.1f,-1000,1000);
        number("Cloud light gain",environment.cloudLightGain,0.05f,0,20);
        number("Cloud ambient gain",environment.cloudAmbientGain,0.05f,0,20);
        int steps=static_cast<int>(environment.cloudSteps);
        if(ImGui::SliderInt("March steps",&steps,0,128)){environment.cloudSteps=static_cast<u32>(steps);changed=true;}RecordEdit();
        steps=static_cast<int>(environment.cloudCoarseSteps);
        if(ImGui::SliderInt("Coarse steps",&steps,1,64)){environment.cloudCoarseSteps=static_cast<u32>(steps);changed=true;}RecordEdit();
        steps=static_cast<int>(environment.cloudLightSteps);
        if(ImGui::SliderInt("Light steps",&steps,0,32)){environment.cloudLightSteps=static_cast<u32>(steps);changed=true;}RecordEdit();
    }
    if(ImGui::CollapsingHeader("Volumetric fog")) {
        number("Fog density",environment.fogDensity,0.0001f,0,1);
        number("Height falloff",environment.fogHeightFalloff,0.001f,0,10);
        number("Base height",environment.fogBaseHeight,0.1f,-10000,10000);
        number("Anisotropy",environment.fogAnisotropy,0.01f,-0.99f,0.99f);
        number("Sun scattering",environment.fogSunScattering,0.01f,0,20);
        number("Ambient scattering",environment.fogAmbientScattering,0.01f,0,20);
        number("Step distance",environment.fogStepDistance,1,0.1f,10000);
        int steps=static_cast<int>(environment.fogSteps);
        if(ImGui::SliderInt("Fog steps",&steps,0,128)){environment.fogSteps=static_cast<u32>(steps);changed=true;}RecordEdit();
    }
    ImGui::PopItemWidth();
    if(changed) {
        try{m_document.Validate();m_sceneDirty=true;Synchronize();}
        catch(const std::exception& error){m_document=before;Report(error);}
    }
}
}
