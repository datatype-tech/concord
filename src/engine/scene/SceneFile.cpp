// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/scene/SceneFile.h"
#include "engine/scene/Box.h"
#include "engine/scene/Camera.h"
#include "engine/scene/PhysicsBody.h"
#include "engine/scene/SunLight.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Concord {
namespace {
struct ParsedObject {
    std::string name = "Box";
    Transform transform{};
    Vec3 size{1.0f, 1.0f, 1.0f};
    Material material{};
    bool visible = true;
    int kind = 0;
    bool castShadow = true;
    f32 mass = 1.0f;
    f32 friction = 0.5f;
    f32 restitution = 0.08f;
    bool lockRotation = false;
};

struct ParsedScene {
    Vec3 cameraPosition{9.0f, 7.0f, 11.0f};
    Vec3 cameraTarget{0.0f, 1.0f, 0.0f};
    f32 fovYDegrees = 60.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;
    f32 orthographicSize = 5.0f;
    bool orthographic = false;
    f32 elevationDegrees = 55.0f;
    f32 azimuthDegrees = 220.0f;
    ColorRGBA sunColor = COLOR_RGB(255, 244, 214);
    f32 sunIntensity = 3.0f;
    bool sunCastShadow = true;
    EnvironmentSettings environment{};
    std::vector<ParsedObject> objects;
};

struct EnvironmentFloat {
    const char* name;
    f32 EnvironmentSettings::* member;
};
constexpr EnvironmentFloat EnvironmentFloats[] = {
    {"cloudCoverage", &EnvironmentSettings::cloudCoverage},
    {"cloudDensity", &EnvironmentSettings::cloudDensity},
    {"cloudAltitude", &EnvironmentSettings::cloudAltitude},
    {"cloudThickness", &EnvironmentSettings::cloudThickness},
    {"cloudWeatherScale", &EnvironmentSettings::cloudWeatherScale},
    {"cloudDetailScale", &EnvironmentSettings::cloudDetailScale},
    {"cloudDetailStrength", &EnvironmentSettings::cloudDetailStrength},
    {"cloudType", &EnvironmentSettings::cloudType},
    {"cloudDriftSpeed", &EnvironmentSettings::cloudDriftSpeed},
    {"cloudLightGain", &EnvironmentSettings::cloudLightGain},
    {"cloudAmbientGain", &EnvironmentSettings::cloudAmbientGain},
    {"sunIntensity", &EnvironmentSettings::sunIntensity},
    {"ambientIntensity", &EnvironmentSettings::ambientIntensity},
    {"exposure", &EnvironmentSettings::exposure},
    {"contrast", &EnvironmentSettings::contrast},
    {"saturation", &EnvironmentSettings::saturation},
    {"vignette", &EnvironmentSettings::vignette},
    {"bloomThreshold", &EnvironmentSettings::bloomThreshold},
    {"bloomIntensity", &EnvironmentSettings::bloomIntensity},
    {"bloomRadius", &EnvironmentSettings::bloomRadius},
    {"chromaticAberration", &EnvironmentSettings::chromaticAberration},
    {"fogDensity", &EnvironmentSettings::fogDensity},
    {"fogHeightFalloff", &EnvironmentSettings::fogHeightFalloff},
    {"fogBaseHeight", &EnvironmentSettings::fogBaseHeight},
    {"fogAnisotropy", &EnvironmentSettings::fogAnisotropy},
    {"fogSunScattering", &EnvironmentSettings::fogSunScattering},
    {"fogAmbientScattering", &EnvironmentSettings::fogAmbientScattering},
    {"fogStepDistance", &EnvironmentSettings::fogStepDistance},
    {"moonIntensity", &EnvironmentSettings::moonIntensity},
};

struct EnvironmentCount {
    const char* name;
    u32 EnvironmentSettings::* member;
};
constexpr EnvironmentCount EnvironmentCounts[] = {
    {"cloudSteps", &EnvironmentSettings::cloudSteps},
    {"cloudCoarseSteps", &EnvironmentSettings::cloudCoarseSteps},
    {"cloudLightSteps", &EnvironmentSettings::cloudLightSteps},
    {"fogSteps", &EnvironmentSettings::fogSteps},
};

void Fail(const char* label)
{
    throw std::runtime_error(std::string("Invalid scene ") + label);
}

void RequireFinite(f32 value, const char* label)
{
    if (!std::isfinite(value)) Fail(label);
}

void ReadVector(std::istream& in, Vec3& value, const char* label)
{
    if (!(in >> value.x >> value.y >> value.z)) Fail(label);
    RequireFinite(value.x, label);
    RequireFinite(value.y, label);
    RequireFinite(value.z, label);
}

void ReadBoolean(std::istream& in, bool& value, const char* label)
{
    int number = -1;
    if (!(in >> number) || (number != 0 && number != 1)) Fail(label);
    value = number == 1;
}

void ReadColor(std::istream& in, ColorRGBA& color, const char* label)
{
    unsigned long long value = 0;
    if (!(in >> value) || value > std::numeric_limits<u32>::max()) Fail(label);
    color = static_cast<ColorRGBA>(value);
}

void Expect(std::istream& in, const char* expected)
{
    std::string token;
    if (!(in >> token) || token != expected) Fail(expected);
}

std::wstring OverridePath(const wchar_t* name)
{
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (length <= 1) return {};
    std::wstring path(length, L'\0');
    if (GetEnvironmentVariableW(name, path.data(), length) == 0) return {};
    if (!path.empty() && path.back() == L'\0') path.pop_back();
    return path;
}

ParsedScene ParseDocument(const std::string& text)
{
    if (text.size() > 8 * 1024 * 1024 || text.find('\0') != std::string::npos)
        Fail("document");
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "CONCORD_SCENE" || version < 1 || version > 4)
        Fail("header");
    ParsedScene parsed;
    if (version >= 3) {
        Expect(in, "camera");
        ReadVector(in, parsed.cameraPosition, "camera position");
        ReadVector(in, parsed.cameraTarget, "camera target");
        if (!(in >> parsed.fovYDegrees >> parsed.nearPlane >> parsed.farPlane >> parsed.orthographicSize))
            Fail("camera");
        ReadBoolean(in, parsed.orthographic, "camera projection");
        Expect(in, "sun");
        if (!(in >> parsed.elevationDegrees >> parsed.azimuthDegrees)) Fail("sun");
        ReadColor(in, parsed.sunColor, "sun color");
        if (!(in >> parsed.sunIntensity)) Fail("sun intensity");
        ReadBoolean(in, parsed.sunCastShadow, "sun shadow");
        Expect(in, "environment");
        ReadVector(in, parsed.environment.zenithColor, "zenith");
        ReadVector(in, parsed.environment.horizonColor, "horizon");
        ReadVector(in, parsed.environment.moonDirection, "moon direction");
        ReadColor(in, parsed.environment.skyColor, "sky color");
        ReadColor(in, parsed.environment.ambientColor, "ambient color");
        for (const auto& field : EnvironmentFloats) {
            if (!(in >> parsed.environment.*field.member)) Fail(field.name);
            RequireFinite(parsed.environment.*field.member, field.name);
        }
        for (const auto& field : EnvironmentCounts) {
            unsigned long long value = 0;
            if (!(in >> value) || value > 256) Fail(field.name);
            parsed.environment.*field.member = static_cast<u32>(value);
        }
        if (version >= 4) {
            Expect(in, "skybox");
            if (!(in >> std::quoted(parsed.environment.skybox)) || parsed.environment.skybox.size() > 1024)
                Fail("skybox");
            for (unsigned char character : parsed.environment.skybox) {
                if (character < 32 || character == 127) Fail("skybox");
            }
        }
        Expect(in, "objects");
    }
    size_t count = 0;
    if (!(in >> count) || count > 10000) Fail("object count");
    parsed.objects.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        ParsedObject object;
        if (!(in >> std::quoted(object.name)) || object.name.empty() || object.name.size() > 255)
            Fail("object name");
        ReadVector(in, object.transform.position, "position");
        ReadVector(in, object.transform.rotation, "rotation");
        ReadVector(in, object.transform.scale, "scale");
        ReadVector(in, object.size, "size");
        ReadColor(in, object.material.albedo, "albedo");
        if (!(in >> object.material.metallic >> object.material.roughness >> object.material.emissive))
            Fail("material");
        if (version >= 2) ReadBoolean(in, object.visible, "visible");
        if (version >= 3) {
            if (!(in >> object.kind) || object.kind < 0 || object.kind > 3) Fail("object kind");
            ReadBoolean(in, object.castShadow, "cast shadow");
            if (!(in >> object.mass >> object.friction >> object.restitution)) Fail("body");
            ReadBoolean(in, object.lockRotation, "lock rotation");
        }
        parsed.objects.push_back(std::move(object));
    }
    in >> std::ws;
    if (!in.eof()) Fail("trailer");
    return parsed;
}

void ClearScene(Scene& scene)
{
    std::vector<Entity> existing;
    scene.Query<Transform>([&](Entity entity, Transform&) { existing.push_back(entity); });
    for (const Entity entity : existing) scene.GetWorld().Destroy(entity);
}

bool SpawnDocument(Scene& scene, const ParsedScene& parsed)
{
    ClearScene(scene);
    scene.SetEnvironment(parsed.environment);
    scene.Spawn<Object::Camera>({
        .position = parsed.cameraPosition,
        .target = parsed.cameraTarget,
        .fovYDegrees = parsed.fovYDegrees,
        .projection = parsed.orthographic ? CameraProjection::Orthographic : CameraProjection::Perspective,
        .orthographicSize = parsed.orthographicSize,
        .nearPlane = parsed.nearPlane,
        .farPlane = parsed.farPlane,
    });
    scene.Spawn<Object::SunLight>({
        .elevationDegrees = parsed.elevationDegrees,
        .azimuthDegrees = parsed.azimuthDegrees,
        .color = parsed.sunColor,
        .intensity = parsed.sunIntensity,
        .castShadow = parsed.sunCastShadow,
    });
    bool physics = false;
    for (const auto& object : parsed.objects) {
        if (object.kind == 3) {
            physics = true;
            auto handle = scene.Spawn<Object::DynamicBox>({
                .transform = object.transform,
                .size = object.size,
                .material = object.material,
                .mass = object.mass,
                .restitution = object.restitution,
                .friction = object.friction,
                .lockRotation = object.lockRotation,
                .visible = object.visible,
                .castShadow = object.castShadow,
            });
            handle.Add<Name>({.value = object.name});
            continue;
        }
        auto handle = scene.Spawn<Object::Box>({
            .transform = object.transform,
            .size = object.size,
            .material = object.material,
            .visible = object.visible,
            .castShadow = object.castShadow,
        });
        handle.Add<Name>({.value = object.name});
        if (object.kind == 2) {
            physics = true;
            handle.Add<Collider>({.shape = CollisionShape::Box, .size = object.size});
            handle.Add<RigidBody>({
                .motion = BodyMotion::Static,
                .mass = object.mass,
                .friction = object.friction,
                .restitution = object.restitution,
                .lockRotation = object.lockRotation,
            });
        }
    }
    return physics;
}

bool LoadPath(Scene& scene, const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path) > 8 * 1024 * 1024) Fail("document");
    std::ifstream file(path, std::ios::binary);
    if (!file) Fail("path");
    const std::string text{std::istreambuf_iterator<char>(file), {}};
    if (file.bad()) Fail("path");
    ParsedScene parsed = ParseDocument(text);
    if (!parsed.environment.skybox.empty()) {
        const std::u8string bytes(reinterpret_cast<const char8_t*>(parsed.environment.skybox.data()),
                                  parsed.environment.skybox.size());
        std::filesystem::path sky{bytes};
        if (sky.is_relative()) sky = path.parent_path() / sky;
        const auto absolute = sky.lexically_normal().u8string();
        parsed.environment.skybox.assign(reinterpret_cast<const char*>(absolute.data()), absolute.size());
    }
    return SpawnDocument(scene, parsed);
}
}

bool LoadSceneOverride(Scene& scene, bool& loaded)
{
    loaded = false;
    const std::wstring path = OverridePath(L"CONCORD_SCENE");
    if (path.empty()) return false;
    loaded = true;
    return LoadPath(scene, path);
}

} // namespace Concord
