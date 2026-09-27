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
    SceneDocument before=m_document;
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
    const bool focus=m_focusWorldSection;m_focusWorldSection=false;
    const auto section=[&](const char* id,WorldSelection which,ImGuiTreeNodeFlags flags) {
        if(focus && m_worldSelection==which)ImGui::SetNextItemOpen(true,ImGuiCond_Always);
        const bool open=Design::Section(TrId(id).c_str(),flags);
        if(focus && m_worldSelection==which && open)ImGui::SetScrollHereY(0.15f);
        return open;
    };
    if(section("Game camera",WorldSelection::Camera,ImGuiTreeNodeFlags_DefaultOpen)) {
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
    if(section("Sun light",WorldSelection::Sun,ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##sun")) {
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
    if(section("Sky and ambient",WorldSelection::Sky,0)) {
        if(Design::Action("##defaultSky","",Tr("Default sky"))){UseDefaultSky();before=m_document;}
        ImGui::SameLine();
        if(Design::Action("##imageSky","plus",Tr(environment.skybox.empty()?"From image...":"Replace skybox")))AddSkybox();
        if(!environment.skybox.empty()) {
            ImGui::SameLine();
            if(Design::Action("##removeSkybox","",Tr("Remove skybox"))) {
                Checkpoint();environment.skybox.clear();m_sceneDirty=true;Synchronize();before=m_document;
            }
            ImGui::TextUnformatted(Utf8Text(Utf8Path(environment.skybox).filename()).c_str());
        }
        if(PropertyGrid::Begin("##sky")) {
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
    }
    if(section("Color and post processing",WorldSelection::None,0) && PropertyGrid::Begin("##post")) {
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
    if(section("Volumetric clouds",WorldSelection::Clouds,0)) {
        if(Design::Action("##defaultClouds","plus",Tr(environment.cloudCoverage>0.001f?"Default clouds":"Add volumetric clouds"))){UseDefaultClouds();before=m_document;}
        if(PropertyGrid::Begin("##clouds")) {
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
    }
    if(section("Volumetric fog",WorldSelection::None,0) && PropertyGrid::Begin("##fog")) {
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
void Workspace::AddSkybox()
{
    if(m_project.empty() || m_scenePath.empty())return;
    const auto chosen=ChooseFile(WideText(Tr("Add skybox")).c_str(),L"Image",L"*.png;*.jpg;*.jpeg;*.tga;*.bmp",m_project);
    if(chosen.empty())return;
    std::error_code error;
    const auto canonical=std::filesystem::weakly_canonical(chosen,error);
    const auto source=error?chosen:canonical;
    error.clear();
    const auto project=std::filesystem::weakly_canonical(m_project,error);
    const auto root=error?m_project:project;
    error.clear();
    const auto projectRelative=std::filesystem::relative(source,root,error);
    const auto projectText=projectRelative.generic_string();
    const bool outside=error || projectText.empty() || projectText==".." || projectText.starts_with("../");
    std::filesystem::path stored=source;
    if(outside) {
        error.clear();
        const auto folder=root/"Skyboxes";
        std::filesystem::create_directories(folder,error);
        if(error){m_status=Tr("Could not copy the skybox into the project");return;}
        const auto destination=folder/source.filename();
        error.clear();
        if(!std::filesystem::equivalent(source,destination,error)) {
            error.clear();
            std::filesystem::copy_file(source,destination,std::filesystem::copy_options::overwrite_existing,error);
            if(error){m_status=Tr("Could not copy the skybox into the project");return;}
        }
        stored=destination;
    }
    error.clear();
    const auto relative=std::filesystem::relative(stored,m_scenePath.parent_path(),error);
    if(error || relative.empty()){m_status=Tr("Could not copy the skybox into the project");return;}
    const auto text=relative.lexically_normal().generic_u8string();
    Checkpoint();
    m_document.environment.skybox.assign(reinterpret_cast<const char*>(text.data()),text.size());
    m_sceneDirty=true;
    SelectWorld(WorldSelection::Sky);
    Synchronize();
}
void Workspace::UseDefaultSky()
{
    Checkpoint();
    const EnvironmentSettings defaults{};
    auto& environment=m_document.environment;
    environment.skybox.clear();
    environment.zenithColor=defaults.zenithColor;
    environment.horizonColor=defaults.horizonColor;
    environment.skyColor=defaults.skyColor;
    SelectWorld(WorldSelection::Sky);
    Synchronize();
}
void Workspace::UseDefaultClouds()
{
    Checkpoint();
    const EnvironmentSettings defaults{};
    auto& environment=m_document.environment;
    environment.cloudCoverage=0.48f;
    environment.cloudDensity=defaults.cloudDensity;
    environment.cloudAltitude=defaults.cloudAltitude;
    environment.cloudThickness=defaults.cloudThickness;
    environment.cloudWeatherScale=defaults.cloudWeatherScale;
    environment.cloudDetailScale=defaults.cloudDetailScale;
    environment.cloudDetailStrength=defaults.cloudDetailStrength;
    environment.cloudType=defaults.cloudType;
    environment.cloudDriftSpeed=defaults.cloudDriftSpeed;
    environment.cloudLightGain=defaults.cloudLightGain;
    environment.cloudAmbientGain=defaults.cloudAmbientGain;
    environment.cloudSteps=defaults.cloudSteps;
    environment.cloudCoarseSteps=defaults.cloudCoarseSteps;
    environment.cloudLightSteps=defaults.cloudLightSteps;
    SelectWorld(WorldSelection::Clouds);
    Synchronize();
}
}
