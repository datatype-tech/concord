// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/SceneDocument.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace Concord::Editor;

namespace {
void Require(bool condition,const char* message)
{
    if(!condition)throw std::runtime_error(message);
}

void RejectWithoutReplacing(SceneDocument& document,const std::string& text)
{
    const auto previous=document.Serialize();
    bool rejected=false;
    try {document.Parse(text);} catch(const std::exception&) {rejected=true;}
    Require(rejected && document.Serialize()==previous,"Invalid document replaced live scene");
}

void CheckLegacyAndVisibility(SceneDocument& document)
{
    const std::string legacyRow="\"Legacy box\" 1 2 3 0 45 0 1 2 1 2 3 4 4294967295 0.25 0.75 1.5";
    SceneDocument legacy;
    legacy.Parse("CONCORD_SCENE 1\n1\n"+legacyRow+"\n");
    Require(legacy.objects.size()==1 && legacy.objects[0].visible,"Version 1 did not default to visible");
    Require(legacy.objects[0].name=="Legacy box" && legacy.objects[0].transform.position.x==1 &&
        legacy.objects[0].transform.scale.y==2 && legacy.objects[0].material.emissive==1.5f,
        "Version 1 lost existing object fields");
    Require(legacy.Serialize().starts_with("CONCORD_SCENE 4\n"),"Legacy scene was not upgraded on save");
    SceneDocument upgraded;upgraded.Parse(legacy.Serialize());
    Require(upgraded.Serialize()==legacy.Serialize(),"Version 1 upgrade did not round trip");

    SceneDocument hidden;
    hidden.Parse("CONCORD_SCENE 2\n1\n"+legacyRow+" 0\n");
    Require(!hidden.objects[0].visible,"Version 2 discarded hidden state");
    Require(hidden.ExportScript().find(".visible=false")!=std::string::npos,"Hidden object was exported as visible");
    Require(legacy.ExportScript().find(".visible=true")!=std::string::npos,"Visible object lost export state");
    for(const auto* invalid:{""," 2"," -1"," true"})
        RejectWithoutReplacing(document,"CONCORD_SCENE 2\n1\n"+legacyRow+invalid+"\n");
    RejectWithoutReplacing(document,"CONCORD_SCENE 1\n1\n"+legacyRow+" 1\n");
    Require(legacy.objects[0].kind==SceneObjectKind::Box && legacy.objects[0].castShadow && legacy.camera.position.x==9,
            "Legacy migration did not preserve default rendering settings");
}
void CheckSettingsAndRecipes(SceneDocument& document)
{
    document.camera.position={5,8,13};document.camera.target={1,2,3};document.camera.fovYDegrees=74;
    document.camera.orthographic=true;document.camera.orthographicSize=18;document.camera.nearPlane=0.2f;document.camera.farPlane=3500;
    document.sun.elevationDegrees=32;document.sun.azimuthDegrees=123;document.sun.intensity=7.25f;document.sun.castShadow=false;
    document.environment.exposure=1.375f;document.environment.cloudCoverage=0.3f;document.environment.fogSteps=8;
    document.environment.skybox="Skyboxes/Day.png";
    document.objects[1].kind=SceneObjectKind::DynamicBox;document.objects[1].mass=3.5f;document.objects[1].lockRotation=true;
    document.objects[2].kind=SceneObjectKind::Plane;document.objects[2].size.y=0.025f;document.objects[2].castShadow=false;
    Require(document.UsesPhysics(),"Physics recipes were not detected");
    SceneDocument restored;restored.Parse(document.Serialize());
    Require(restored.Serialize()==document.Serialize(),"Camera, sun, environment, skybox or object recipe did not round trip");
    Require(restored.environment.skybox=="Skyboxes/Day.png","Skybox path did not round trip");
    const auto script=restored.ExportScript();
    for(const auto* fragment:{".fovYDegrees=74.","CameraProjection::Orthographic",".elevationDegrees=32.",".intensity=7.25",
                             "environment.exposure=1.375","environment.fogSteps=8u","environment.skybox=\"Skyboxes/Day.png\"","Object::DynamicBox","BodyMotion::Static",".mass=3.5",".lockRotation=true"})
        Require(script.find(fragment)!=std::string::npos,"Generated scene omitted an authored setting or physics component");
    auto header=document.Serialize();header.resize(header.find("\nobjects "));
    const std::string row="\"Bad recipe\" 0 0 0 0 0 0 1 1 1 1 1 1 4294967295 0 1 0 1 ";
    for(const auto* tail:{"9 1 1 0.5 0.08 0","0 2 1 0.5 0.08 0","0 1 -1 0.5 0.08 0","0 1 1 nan 0.08 0","0 1 1 0.5 2 0"})
        RejectWithoutReplacing(document,header+"\nobjects 1\n"+row+tail+"\n");
    auto corrupt=document.Serialize();corrupt.replace(corrupt.find("camera "),7,"camera nan ");RejectWithoutReplacing(document,corrupt);
    auto invalid=document;invalid.camera.farPlane=invalid.camera.nearPlane;
    bool rejected=false;try {invalid.Serialize();}catch(const std::exception&){rejected=true;}
    Require(rejected,"Save accepted an invalid clip range");
    invalid=document;invalid.environment.skybox="C:/outside.png";rejected=false;
    try {invalid.Serialize();}catch(const std::exception&){rejected=true;}
    Require(rejected,"Save accepted an absolute skybox path");
    invalid=document;invalid.environment.exposure=std::numeric_limits<float>::infinity();rejected=false;
    try {invalid.ExportScript();}catch(const std::exception&){rejected=true;}
    Require(rejected,"Export accepted non-finite environment settings");
}
}

int main(int argc,char** argv)
{
    try {
        auto document=SceneDocument::Starter();document.objects[1].name="Quoted \"name\" \\ path";
        document.objects[1].visible=false;
        auto text=document.Serialize();SceneDocument copy;copy.Parse(text);
        Require(copy.Serialize()==text && !copy.objects[1].visible && copy.objects[0].visible,
            "Round trip lost scene data or visibility");
        for(const auto& malformed:{"CONCORD_SCENE 9\n0\n","CONCORD_SCENE 1\n10001\n","CONCORD_SCENE 1\n1\n\"Box\" nan 0 0","CONCORD_SCENE 1\n0\ntrailing"}) {
            RejectWithoutReplacing(copy,malformed);
        }
        CheckLegacyAndVisibility(copy);
        CheckSettingsAndRecipes(document);
        if(argc>1) {
            auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
            WriteText(directory/"Scene.scene",document.Serialize());
            WriteText(directory/"SceneLayout.cx",document.ExportScript());
            WriteText(directory/"Main.cx",StarterScript());
        }
        std::cout<<"Scene v4 settings/physics, skybox, v1/v2 migration, round trip, bounded parsing, transactional rejection and export passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
