// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/SceneDocument.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace Concord::Editor {
namespace {
struct EnvironmentFloat {
    const char* name;
    float EnvironmentSettings::*member;
    float minimum,maximum;
};
constexpr EnvironmentFloat EnvironmentFloats[]={
    {"cloudCoverage",&EnvironmentSettings::cloudCoverage,0,1},{"cloudDensity",&EnvironmentSettings::cloudDensity,0,10},
    {"cloudAltitude",&EnvironmentSettings::cloudAltitude,-100000,100000},{"cloudThickness",&EnvironmentSettings::cloudThickness,0.001f,100000},
    {"cloudWeatherScale",&EnvironmentSettings::cloudWeatherScale,0.001f,100000},{"cloudDetailScale",&EnvironmentSettings::cloudDetailScale,0.001f,100000},
    {"cloudDetailStrength",&EnvironmentSettings::cloudDetailStrength,0,1},{"cloudType",&EnvironmentSettings::cloudType,0,1},
    {"cloudDriftSpeed",&EnvironmentSettings::cloudDriftSpeed,-10000,10000},{"cloudLightGain",&EnvironmentSettings::cloudLightGain,0,100},
    {"cloudAmbientGain",&EnvironmentSettings::cloudAmbientGain,0,100},{"sunIntensity",&EnvironmentSettings::sunIntensity,0,100},
    {"ambientIntensity",&EnvironmentSettings::ambientIntensity,0,100},{"exposure",&EnvironmentSettings::exposure,0,100},
    {"contrast",&EnvironmentSettings::contrast,0,2},{"saturation",&EnvironmentSettings::saturation,0,10},
    {"vignette",&EnvironmentSettings::vignette,0,1},{"bloomThreshold",&EnvironmentSettings::bloomThreshold,0,1000},
    {"bloomIntensity",&EnvironmentSettings::bloomIntensity,0,100},{"bloomRadius",&EnvironmentSettings::bloomRadius,0,1000},
    {"chromaticAberration",&EnvironmentSettings::chromaticAberration,0,1},{"fogDensity",&EnvironmentSettings::fogDensity,0,10},
    {"fogHeightFalloff",&EnvironmentSettings::fogHeightFalloff,0,100},{"fogBaseHeight",&EnvironmentSettings::fogBaseHeight,-100000,100000},
    {"fogAnisotropy",&EnvironmentSettings::fogAnisotropy,-0.99f,0.99f},{"fogSunScattering",&EnvironmentSettings::fogSunScattering,0,100},
    {"fogAmbientScattering",&EnvironmentSettings::fogAmbientScattering,0,100},{"fogStepDistance",&EnvironmentSettings::fogStepDistance,0.001f,100000},
    {"moonIntensity",&EnvironmentSettings::moonIntensity,0,1}
};
struct EnvironmentCount {const char* name;u32 EnvironmentSettings::*member;};
constexpr EnvironmentCount EnvironmentCounts[]={
    {"cloudSteps",&EnvironmentSettings::cloudSteps},{"cloudCoarseSteps",&EnvironmentSettings::cloudCoarseSteps},
    {"cloudLightSteps",&EnvironmentSettings::cloudLightSteps},{"fogSteps",&EnvironmentSettings::fogSteps}
};
void CheckNumber(float value,float minimum,float maximum,const char* label)
{
    if(!std::isfinite(value) || value<minimum || value>maximum)throw std::runtime_error(std::string("Invalid scene ")+label);
}
void CheckVector(Vec3 value,float minimum,float maximum,const char* label)
{
    for(float number:{value.x,value.y,value.z})CheckNumber(number,minimum,maximum,label);
}
void WriteVector(std::ostream& out,Vec3 value){out<<value.x<<' '<<value.y<<' '<<value.z;}
void ReadVector(std::istream& in,Vec3& value)
{
    if(!(in>>value.x>>value.y>>value.z))throw std::runtime_error("Invalid scene vector");
}
void ReadBoolean(std::istream& in,bool& value)
{
    int number=-1;if(!(in>>number) || (number!=0 && number!=1))throw std::runtime_error("Invalid scene boolean");value=number==1;
}
void ReadColor(std::istream& in,ColorRGBA& color)
{
    unsigned long long value=0;if(!(in>>value) || value>std::numeric_limits<u32>::max())throw std::runtime_error("Invalid scene color");
    color=static_cast<ColorRGBA>(value);
}
void Expect(std::istream& in,const char* expected)
{
    std::string token;if(!(in>>token) || token!=expected)throw std::runtime_error(std::string("Missing scene section: ")+expected);
}
}
std::string ReadText(const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path)>8*1024*1024) throw std::runtime_error("File exceeds 8 MiB editor limit");
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read "+path.string());
    return {std::istreambuf_iterator<char>(stream),{}};
}
void WriteText(const std::filesystem::path& path,const std::string& text)
{
    auto temporary=path; temporary += L".tmp";
    { std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);
      stream.write(text.data(),static_cast<std::streamsize>(text.size())); stream.flush();
      if (!stream) throw std::runtime_error("Cannot save "+path.string()); }
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace "+path.string());
}
SceneDocument SceneDocument::Starter()
{
    SceneDocument document;
    document.objects={
        {.name="Stage",.transform={.position={0,-0.3f,0}},.size={16,0.5f,14},.material={.albedo=COLOR_RGB(52,63,79)},.kind=SceneObjectKind::StaticBox},
        {.name="Concord",.transform={.position={0,1.2f,0},.rotation={0,25,0}},.size={2,2.4f,2}},
        {.name="Amber",.transform={.position={-3,0.75f,1}},.size={1.5f,1.5f,1.5f},.material={.albedo=COLOR_RGB(242,162,75),.metallic=0.3f}},
        {.name="Violet",.transform={.position={3,1.8f,-1},.rotation={0,-20,0}},.size={1.4f,3.6f,1.4f},.material={.albedo=COLOR_RGB(146,115,238)}},
    };
    return document;
}
std::string SceneDocument::Serialize() const
{
    Validate();std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"CONCORD_SCENE 4\ncamera ";
    WriteVector(out,camera.position);out<<' ';WriteVector(out,camera.target);
    out<<' '<<camera.fovYDegrees<<' '<<camera.nearPlane<<' '<<camera.farPlane<<' '<<camera.orthographicSize<<' '<<camera.orthographic
       <<"\nsun "<<sun.elevationDegrees<<' '<<sun.azimuthDegrees<<' '<<sun.color<<' '<<sun.intensity<<' '<<sun.castShadow<<"\nenvironment ";
    WriteVector(out,environment.zenithColor);out<<' ';WriteVector(out,environment.horizonColor);out<<' ';WriteVector(out,environment.moonDirection);
    out<<' '<<environment.skyColor<<' '<<environment.ambientColor;
    for(const auto& field:EnvironmentFloats)out<<' '<<environment.*field.member;
    for(const auto& field:EnvironmentCounts)out<<' '<<environment.*field.member;
    out<<"\nskybox "<<std::quoted(environment.skybox)<<"\nobjects "<<objects.size()<<'\n';
    for (const auto& object:objects) {
        out<<std::quoted(object.name);
        for (auto v:{object.transform.position,object.transform.rotation,object.transform.scale,object.size})
            out<<' '<<v.x<<' '<<v.y<<' '<<v.z;
        out<<' '<<object.material.albedo<<' '<<object.material.metallic<<' '<<object.material.roughness<<' '<<object.material.emissive<<' '<<object.visible
           <<' '<<static_cast<int>(object.kind)<<' '<<object.castShadow<<' '<<object.mass<<' '<<object.friction<<' '<<object.restitution<<' '<<object.lockRotation<<'\n';
    }
    return out.str();
}
void SceneDocument::Parse(const std::string& text)
{
    if(text.size()>8*1024*1024 || text.find('\0')!=std::string::npos)throw std::runtime_error("Scene document is too large or contains a null byte");
    std::istringstream in(text); in.imbue(std::locale::classic());
    std::string magic; int version=0; size_t count=0;
    if (!(in>>magic>>version) || magic!="CONCORD_SCENE" || version<1 || version>4)
        throw std::runtime_error("Unsupported or corrupt scene header");
    SceneDocument parsed;
    if(version>=3) {
        Expect(in,"camera");ReadVector(in,parsed.camera.position);ReadVector(in,parsed.camera.target);
        in>>parsed.camera.fovYDegrees>>parsed.camera.nearPlane>>parsed.camera.farPlane>>parsed.camera.orthographicSize;ReadBoolean(in,parsed.camera.orthographic);
        Expect(in,"sun");in>>parsed.sun.elevationDegrees>>parsed.sun.azimuthDegrees;ReadColor(in,parsed.sun.color);
        in>>parsed.sun.intensity;ReadBoolean(in,parsed.sun.castShadow);
        Expect(in,"environment");ReadVector(in,parsed.environment.zenithColor);ReadVector(in,parsed.environment.horizonColor);ReadVector(in,parsed.environment.moonDirection);
        ReadColor(in,parsed.environment.skyColor);ReadColor(in,parsed.environment.ambientColor);
        for(const auto& field:EnvironmentFloats)in>>parsed.environment.*field.member;
        for(const auto& field:EnvironmentCounts) {
            unsigned long long value=0;if(!(in>>value) || value>256)throw std::runtime_error("Invalid scene environment step count");
            parsed.environment.*field.member=static_cast<u32>(value);
        }
        if(version>=4) {
            Expect(in,"skybox");
            if(!(in>>std::quoted(parsed.environment.skybox)) || parsed.environment.skybox.size()>1024)
                throw std::runtime_error("Invalid skybox path");
        }
        Expect(in,"objects");
    }
    if(!(in>>count) || count>10000)throw std::runtime_error("Invalid scene object count");
    for(size_t index=0;index<count;++index) {
        SceneObject object;
        if (!(in>>std::quoted(object.name)) || object.name.size()>255 || object.name.find_first_of("\r\n")!=std::string::npos)
            throw std::runtime_error("Invalid scene object name");
        for (Vec3* value:{&object.transform.position,&object.transform.rotation,&object.transform.scale,&object.size})ReadVector(in,*value);
        auto& material=object.material;
        ReadColor(in,material.albedo);
        if(!(in>>material.metallic>>material.roughness>>material.emissive))throw std::runtime_error("Invalid scene material");
        if(version>=2)ReadBoolean(in,object.visible);
        if(version>=3) {
            int kind=0;if(!(in>>kind) || kind<0 || kind>3)throw std::runtime_error("Unsupported scene object kind");
            object.kind=static_cast<SceneObjectKind>(kind);ReadBoolean(in,object.castShadow);
            in>>object.mass>>object.friction>>object.restitution;ReadBoolean(in,object.lockRotation);
        }
        parsed.objects.push_back(std::move(object));
    }
    in>>std::ws;
    if (!in.eof()) throw std::runtime_error("Unexpected data after scene objects");
    parsed.Validate();*this=std::move(parsed);
}
void SceneDocument::Validate() const
{
    if(objects.size()>10000)throw std::runtime_error("Scene supports at most 10000 objects");
    CheckVector(camera.position,-100000,100000,"camera position");CheckVector(camera.target,-100000,100000,"camera target");
    CheckNumber(camera.fovYDegrees,1,179,"camera field of view");CheckNumber(camera.nearPlane,0.001f,100000,"camera near plane");
    CheckNumber(camera.farPlane,camera.nearPlane+0.001f,1000000,"camera far plane");CheckNumber(camera.orthographicSize,0.001f,100000,"orthographic size");
    if(camera.farPlane<=camera.nearPlane)throw std::runtime_error("Camera far plane must be greater than near plane");
    CheckNumber(sun.elevationDegrees,-90,90,"sun elevation");CheckNumber(sun.azimuthDegrees,-36000,36000,"sun azimuth");CheckNumber(sun.intensity,0,1000,"sun intensity");
    CheckVector(environment.zenithColor,0,100,"zenith color");CheckVector(environment.horizonColor,0,100,"horizon color");
    CheckVector(environment.moonDirection,-1,1,"moon direction");
    if(Dot(environment.moonDirection,environment.moonDirection)<0.000001f)throw std::runtime_error("Moon direction must not be zero");
    for(const auto& field:EnvironmentFloats)CheckNumber(environment.*field.member,field.minimum,field.maximum,field.name);
    for(const auto& field:EnvironmentCounts)if(environment.*field.member>256)throw std::runtime_error("Environment step count exceeds 256");
    if(environment.skybox.size()>1024)throw std::runtime_error("Skybox path is too long");
    for(unsigned char c:environment.skybox)if(c<32 || c==127)throw std::runtime_error("Skybox path contains a control character");
    const auto skyboxPath=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(environment.skybox.data()),environment.skybox.size()));
    if(!skyboxPath.empty() && skyboxPath.is_absolute())throw std::runtime_error("Skybox path must stay inside the project");
    for(const auto& object:objects) {
        if(object.name.empty() || object.name.size()>255)throw std::runtime_error("Scene object name must contain between 1 and 255 bytes");
        for(unsigned char c:object.name)if(c<32 || c==127)throw std::runtime_error("Scene object name contains a control character");
        CheckVector(object.transform.position,-100000,100000,"position");CheckVector(object.transform.rotation,-100000,100000,"rotation");
        CheckVector(object.transform.scale,0.001f,100000,"scale");CheckVector(object.size,0.001f,100000,"size");
        CheckNumber(object.material.metallic,0,1,"metallic");CheckNumber(object.material.roughness,0,1,"roughness");CheckNumber(object.material.emissive,0,100,"emission");
        if(static_cast<int>(object.kind)<0 || static_cast<int>(object.kind)>3)throw std::runtime_error("Unsupported scene object kind");
        CheckNumber(object.mass,0.001f,1000000,"body mass");CheckNumber(object.friction,0,1,"body friction");CheckNumber(object.restitution,0,1,"body restitution");
    }
}
bool SceneDocument::UsesPhysics() const
{
    return std::any_of(objects.begin(),objects.end(),[](const auto& object){return object.kind==SceneObjectKind::StaticBox || object.kind==SceneObjectKind::DynamicBox;});
}
std::string SceneDocument::ExportScript() const
{
    Validate();std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(9);
    out<<"// Generated by Concord Editor. Edit the .scene file through the 3D viewport.\n"
          "use Concord.CScene;\nuse Concord.CObject;\nuse Concord.CCamera;\nuse Concord.CLight;\n";
    if(UsesPhysics())out<<"use Concord.CPhysics;\n";
    out<<"class Layout {\npublic:\n    static bool UsesPhysics() { return "<<(UsesPhysics()?"true":"false")<<"; }\n"
          "    static void Build(Concord::Scene& scene) {\n";
    auto vector=[&out](Vec3 value) {out<<'{'<<value.x<<"f,"<<value.y<<"f,"<<value.z<<"f}";};
    out<<"        scene.Spawn<Concord::Object::Camera>({.position=";vector(camera.position);out<<",.target=";vector(camera.target);
    out<<",.fovYDegrees="<<camera.fovYDegrees<<"f,.projection=Concord::CameraProjection::"<<(camera.orthographic?"Orthographic":"Perspective")
       <<",.orthographicSize="<<camera.orthographicSize<<"f,.nearPlane="<<camera.nearPlane<<"f,.farPlane="<<camera.farPlane<<"f});\n"
       <<"        scene.Spawn<Concord::Object::SunLight>({.elevationDegrees="<<sun.elevationDegrees<<"f,.azimuthDegrees="<<sun.azimuthDegrees
       <<"f,.color="<<sun.color<<"u,.intensity="<<sun.intensity<<"f,.castShadow="<<(sun.castShadow?"true":"false")<<"});\n"
       <<"        Concord::EnvironmentSettings environment;\n        environment.zenithColor=";vector(environment.zenithColor);
    out<<";\n        environment.horizonColor=";vector(environment.horizonColor);out<<";\n        environment.moonDirection=";vector(environment.moonDirection);
    out<<";\n        environment.skyColor="<<environment.skyColor<<"u;\n        environment.ambientColor="<<environment.ambientColor<<"u;\n";
    out<<"        environment.skybox="<<std::quoted(environment.skybox)<<";\n";
    for(const auto& field:EnvironmentFloats)out<<"        environment."<<field.name<<'='<<environment.*field.member<<"f;\n";
    for(const auto& field:EnvironmentCounts)out<<"        environment."<<field.name<<'='<<environment.*field.member<<"u;\n";
    out<<"        scene.SetEnvironment(environment);\n";
    size_t index=0;
    for (const auto& object:objects) {
        const auto name="object"+std::to_string(index++);
        out<<"        auto "<<name<<"=scene.Spawn<Concord::Object::"<<(object.kind==SceneObjectKind::DynamicBox?"DynamicBox":"Box")<<">({.transform={.position=";vector(object.transform.position);
        out<<",.rotation=";vector(object.transform.rotation);out<<",.scale=";vector(object.transform.scale);
        out<<"},.size=";vector(object.size);
        out<<",.material={.albedo="<<object.material.albedo<<"u,.metallic="<<object.material.metallic<<"f,.roughness="
           <<object.material.roughness<<"f,.emissive="<<object.material.emissive<<"f}";
        if(object.kind==SceneObjectKind::DynamicBox)out<<",.mass="<<object.mass<<"f,.restitution="<<object.restitution<<"f,.friction="<<object.friction<<"f,.lockRotation="<<(object.lockRotation?"true":"false");
        out<<",.visible="<<(object.visible?"true":"false")<<",.castShadow="<<(object.castShadow?"true":"false")<<"});\n";
        out<<"        "<<name<<".Add<Concord::Name>({.value="<<std::quoted(object.name)<<"});\n";
        if(object.kind==SceneObjectKind::StaticBox) {
            out<<"        "<<name<<".Add<Concord::Collider>({.shape=Concord::CollisionShape::Box,.size=";vector(object.size);out<<"});\n"
               <<"        "<<name<<".Add<Concord::RigidBody>({.motion=Concord::BodyMotion::Static,.mass="<<object.mass<<"f,.friction="<<object.friction
               <<"f,.restitution="<<object.restitution<<"f,.lockRotation="<<(object.lockRotation?"true":"false")<<"});\n";
        }
    }
    out<<"    }\n};\n";
    return out.str();
}
std::string StarterScript()
{
    return R"(use Concord.CApplication;
use Concord.CScene;
use Project.SceneLayout;
@include(path="cstdlib", system=true);

@entry {
    Concord::Scene scene;
    Concord::Window window({.title="My Concord Game", .resolution={1280,800}});
    Concord::Game game({.enableRayTracing=false, .frameRateLimit=120});
    game.AttachWindow(window);
    SceneLayout::Layout::Build(scene);
    game.LoadScene(scene);
    // Add your game logic here. SceneLayout.cx is generated from the 3D layout.
    game.OnUpdate([&](float dt) {
        if (std::getenv("CONCORD_EDITOR_GAME_SMOKE") && game.FrameCount()>60) game.Quit();
    });
    game.Run();
    return 0;
};
)";
}
}
