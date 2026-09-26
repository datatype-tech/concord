// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include "editor/PropertyGrid.h"
#include <algorithm>

namespace Concord::Editor {
void Workspace::SceneSettings()
{
    const SceneDocument before=m_document;
    bool changed=false;
    const auto number=[&](const char* label,float& value,float step,float low,float high) {
        changed|=PropertyGrid::Float(label,value,step,low,high);RecordEdit();
    };
    const auto steps=[&](const char* label,u32& value,int high,int low=0) {
        int count=static_cast<int>(value);
        if(PropertyGrid::Integer(label,count,low,high)){value=static_cast<u32>(count);changed=true;}
        RecordEdit();
    };
    const auto relative=Utf8Text(m_scenePath.lexically_relative(m_project));
    Design::Image("cube",ImGui::GetFontSize(),ImGui::GetColorU32(Design::Accent));ImGui::SameLine();
    ImGui::TextUnformatted(relative.c_str());
    const bool initial=Utf8Path(m_projectConfig.startupScene).make_preferred().lexically_normal()==m_scenePath.lexically_relative(m_project).lexically_normal();
    if(initial)Design::Badge(Tr("Initial scene"),{0.45f,0.80f,0.52f,1});
    else if(Design::Action("##makeInitial","play",Tr("Set as initial scene")))Attempt([&]{
        auto settings=m_projectConfig;settings.startupScene=relative;ApplyProjectSettings(settings);m_status=Tr("Initial scene saved");
    });
    ImGui::Spacing();
    if(ImGui::CollapsingHeader(TrId("Game camera").c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& camera=m_document.camera;
        if(Design::Action("##useView","eye",Tr("Use current 3D view"))){Checkpoint();camera.position=m_eye;camera.target=m_target;changed=true;}
        if(PropertyGrid::Begin("##camera")) {
            changed|=PropertyGrid::Vector("Position",camera.position,0.1f,-10000,10000);RecordEdit();
            changed|=PropertyGrid::Vector("Look at",camera.target,0.1f,-10000,10000);RecordEdit();
            const char* projections[]={"Perspective","Orthographic"};
            int projection=camera.orthographic?1:0;
            if(PropertyGrid::Combo("Projection",projection,projections,2)){Checkpoint();camera.orthographic=projection==1;changed=true;}
            if(camera.orthographic)number("Orthographic size",camera.orthographicSize,0.1f,0.01f,10000);
            else {changed|=PropertyGrid::Slider("Field of view",camera.fovYDegrees,5,160,"%.0f deg");RecordEdit();}
            number("Near plane",camera.nearPlane,0.01f,0.001f,camera.farPlane-0.01f);
            number("Far plane",camera.farPlane,1,camera.nearPlane+0.01f,100000);
            PropertyGrid::End();
        }
    }
    if(ImGui::CollapsingHeader(TrId("Sun light").c_str(),ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##sun")) {
        auto& sun=m_document.sun;
        changed|=PropertyGrid::Slider("Elevation",sun.elevationDegrees,-90,90,"%.1f deg");RecordEdit();
        changed|=PropertyGrid::Slider("Azimuth",sun.azimuthDegrees,-360,360,"%.1f deg");RecordEdit();
        number("Intensity",sun.intensity,0.05f,0,100);
        changed|=PropertyGrid::Color("Color",sun.color);RecordEdit();
        bool shadow=sun.castShadow;
        if(PropertyGrid::Check("Cast shadows",shadow)){Checkpoint();sun.castShadow=shadow;changed=true;}
        PropertyGrid::End();
    }
    auto& environment=m_document.environment;
    if(ImGui::CollapsingHeader(TrId("Sky and ambient").c_str()) && PropertyGrid::Begin("##sky")) {
        changed|=PropertyGrid::ColorFloat("Zenith",environment.zenithColor);RecordEdit();
        changed|=PropertyGrid::ColorFloat("Horizon",environment.horizonColor);RecordEdit();
        changed|=PropertyGrid::Color("Clear color",environment.skyColor);RecordEdit();
        changed|=PropertyGrid::Color("Ambient color",environment.ambientColor);RecordEdit();
        number("Ambient intensity",environment.ambientIntensity,0.01f,0,10);
        number("Sun multiplier",environment.sunIntensity,0.01f,0,20);
        number("Moon intensity",environment.moonIntensity,0.01f,0,1);
        changed|=PropertyGrid::Vector("Moon direction",environment.moonDirection,0.01f,-1,1);RecordEdit();
        PropertyGrid::End();
    }
    if(ImGui::CollapsingHeader(TrId("Color and post processing").c_str()) && PropertyGrid::Begin("##post")) {
        number("Exposure",environment.exposure,0.01f,0.01f,20);
        number("Contrast",environment.contrast,0.01f,0,2);
        number("Saturation",environment.saturation,0.01f,0,3);
        number("Vignette",environment.vignette,0.01f,0,1);
        number("Bloom threshold",environment.bloomThreshold,0.1f,0,100);
        number("Bloom intensity",environment.bloomIntensity,0.01f,0,10);
        number("Bloom radius",environment.bloomRadius,0.5f,0,100);
        number("Chromatic aberration",environment.chromaticAberration,0.001f,0,0.1f);
        PropertyGrid::End();
    }
    if(ImGui::CollapsingHeader(TrId("Volumetric clouds").c_str()) && PropertyGrid::Begin("##clouds")) {
        number("Coverage",environment.cloudCoverage,0.01f,0,1);
        number("Density",environment.cloudDensity,0.0001f,0,1);
        number("Altitude",environment.cloudAltitude,1,-10000,10000);
        number("Thickness",environment.cloudThickness,1,1,10000);
        number("Weather scale",environment.cloudWeatherScale,1,1,10000);
        number("Detail scale",environment.cloudDetailScale,1,1,10000);
        number("Detail strength",environment.cloudDetailStrength,0.01f,0,1);
        number("Cloud type",environment.cloudType,0.01f,0,1);
        number("Drift speed",environment.cloudDriftSpeed,0.1f,-1000,1000);
        number("Light gain",environment.cloudLightGain,0.05f,0,20);
        number("Ambient gain",environment.cloudAmbientGain,0.05f,0,20);
        steps("March steps",environment.cloudSteps,128);
        steps("Coarse steps",environment.cloudCoarseSteps,64,1);
        steps("Light steps",environment.cloudLightSteps,32);
        PropertyGrid::End();
    }
    if(ImGui::CollapsingHeader(TrId("Volumetric fog").c_str()) && PropertyGrid::Begin("##fog")) {
        number("Fog density",environment.fogDensity,0.0001f,0,1);
        number("Height falloff",environment.fogHeightFalloff,0.001f,0,10);
        number("Base height",environment.fogBaseHeight,0.1f,-10000,10000);
        number("Anisotropy",environment.fogAnisotropy,0.01f,-0.99f,0.99f);
        number("Sun scattering",environment.fogSunScattering,0.01f,0,20);
        number("Ambient scattering",environment.fogAmbientScattering,0.01f,0,20);
        number("Step distance",environment.fogStepDistance,1,0.1f,10000);
        steps("Fog steps",environment.fogSteps,128);
        PropertyGrid::End();
    }
    ImGui::PushTextWrapPos();
    ImGui::TextColored(Design::Muted,"%s",Tr("Clouds, fog and post processing are saved and exported to the game; the editor preview uses the raster path."));
    ImGui::PopTextWrapPos();
    if(changed) {
        try{m_document.Validate();m_sceneDirty=true;Synchronize();}
        catch(const std::exception& error){m_document=before;Report(error);}
    }
}
}
