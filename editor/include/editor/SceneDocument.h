// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORDOCUMENT_H
#define CONCORD_EDITORDOCUMENT_H
#include <Concord/CScene.h>
#include <filesystem>
#include <string>
#include <vector>

namespace Concord::Editor {
/** Supported rendered recipes. Plane is a finite thin box; physics boxes use Jolt. */
enum class SceneObjectKind { Box, Plane, StaticBox, DynamicBox };
/** An authored object; runtime entity handles never enter the persisted document. */
struct SceneObject {
    std::string name = "Box";
    Transform transform{};
    Vec3 size{1,1,1};
    Material material{.albedo=COLOR_RGB(62,190,181)};
    bool visible=true;
    SceneObjectKind kind=SceneObjectKind::Box;
    bool castShadow=true;
    float mass=1.0f,friction=0.5f,restitution=0.08f;
    bool lockRotation=false;
};
/** Authored game camera, independent from the editor's orbit camera. */
struct SceneCameraSettings {
    Vec3 position{9,7,11},target{0,1,0};
    float fovYDegrees=60,nearPlane=0.1f,farPlane=1000,orthographicSize=5;
    bool orthographic=false;
};
/** Directional light exported into the running game and reflected by the scene preview. */
struct SceneSunSettings {
    float elevationDegrees=55,azimuthDegrees=220;
    ColorRGBA color=COLOR_RGB(255,244,214);
    float intensity=3;
    bool castShadow=true;
};
/** Versioned scene document and deterministic ConcordScript exporter. */
class SceneDocument {
public:
    std::vector<SceneObject> objects;
    SceneCameraSettings camera;
    SceneSunSettings sun;
    EnvironmentSettings environment;
    /** Starter stage shared by the editor and generated game. */
    static SceneDocument Starter();
    std::string Serialize() const;
    /** Throws on invalid input; leaves this document untouched on failure. */
    void Parse(const std::string& text);
    /** Rejects unsupported types and non-finite or out-of-range settings before save/export. */
    void Validate() const;
    [[nodiscard]] bool UsesPhysics() const;
    std::string ExportScript() const;
};
/** Reads a bounded UTF-8 source/document file. */
std::string ReadText(const std::filesystem::path& path);
/** Replaces a file only after its complete sibling temporary file was flushed. */
void WriteText(const std::filesystem::path& path,const std::string& text);
/** Main script for a newly created editor project. */
std::string StarterScript();
}
#endif
