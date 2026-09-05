// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ModelLoader.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.0001f;
}

void AppendFloats(std::vector<unsigned char>& bytes, std::initializer_list<Concord::f32> values)
{
    for (const Concord::f32 value : values) {
        unsigned char encoded[4];
        std::memcpy(encoded, &value, sizeof(encoded));
        bytes.insert(bytes.end(), encoded, encoded + sizeof(encoded));
    }
}

void AppendU16(std::vector<unsigned char>& bytes, std::initializer_list<unsigned short> values)
{
    for (const unsigned short value : values) {
        bytes.push_back(static_cast<unsigned char>(value & 0xFFu));
        bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xFFu));
    }
}

std::string Base64(const std::vector<unsigned char>& bytes)
{
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    for (Concord::usize index = 0; index < bytes.size(); index += 3) {
        const Concord::u32 chunk = (static_cast<Concord::u32>(bytes[index]) << 16) |
                          (index + 1 < bytes.size() ? static_cast<Concord::u32>(bytes[index + 1]) << 8 : 0) |
                          (index + 2 < bytes.size() ? static_cast<Concord::u32>(bytes[index + 2]) : 0);
        out += kTable[(chunk >> 18) & 63];
        out += kTable[(chunk >> 12) & 63];
        out += index + 1 < bytes.size() ? kTable[(chunk >> 6) & 63] : '=';
        out += index + 2 < bytes.size() ? kTable[chunk & 63] : '=';
    }
    return out;
}

std::string DataUri(const std::vector<unsigned char>& bytes)
{
    return "data:application/octet-stream;base64," + Base64(bytes);
}

std::string WithData(const std::string& json, const std::vector<unsigned char>& bytes)
{
    std::string out = json;
    const std::string uri = DataUri(bytes);
    const std::string length = std::to_string(bytes.size());
    for (Concord::usize at = out.find("@DATA@"); at != std::string::npos; at = out.find("@DATA@")) {
        out.replace(at, 6, uri);
    }
    for (Concord::usize at = out.find("@LENGTH@"); at != std::string::npos; at = out.find("@LENGTH@")) {
        out.replace(at, 8, length);
    }
    return out;
}

Concord::ModelLoadResult Load(const std::string& json, const Concord::ModelLoadOptions& options)
{
    return Concord::ModelLoader::LoadGltf(json, {}, options);
}

/** One-joint rig with an inverse-bind and a two-key hips translation clip. */
const char* kScaleJson = R"json({
  "asset": {"version": "2.0"},
  "nodes": [{"name": "Hips", "translation": [2.0, 0.0, 0.0]}],
  "skins": [{"joints": [0], "inverseBindMatrices": 0}],
  "animations": [{"name": "move",
    "samplers": [{"input": 1, "output": 2}],
    "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 1, "type": "MAT4"},
    {"bufferView": 1, "componentType": 5126, "count": 2, "type": "SCALAR"},
    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "VEC3"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 64},
    {"buffer": 0, "byteOffset": 64, "byteLength": 8},
    {"buffer": 0, "byteOffset": 72, "byteLength": 24}],
  "buffers": [{"uri": "@DATA@", "byteLength": @LENGTH@}]
})json";

bool TestImportScaleConsistency()
{
    std::vector<unsigned char> bytes;
    AppendFloats(bytes, {1.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 1.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 1.0f, 0.0f,
                         -2.0f, 0.0f, 0.0f, 1.0f,
                         0.0f, 1.0f,
                         4.0f, 0.0f, 0.0f,
                         8.0f, 0.0f, 0.0f});
    const std::string json = WithData(kScaleJson, bytes);

    const Concord::ModelLoadResult unit = Load(json, {});
    if (!unit.Succeeded() || unit.asset.skeletons.size() != 1) {
        std::printf("  unit load failed: %s\n", unit.error.message.c_str());
        return false;
    }
    if (!Near(unit.asset.skeletons[0].joints[0].inverseBind.col[3].x, -2.0f)) return false;
    if (!Near(unit.asset.animations[0].channels[0].vec3Keys[1].value.x, 8.0f)) return false;

    const Concord::ModelLoadResult scaled = Load(json, {.scale = 0.5f});
    if (!scaled.Succeeded()) {
        std::printf("  scaled load failed: %s\n", scaled.error.message.c_str());
        return false;
    }
    const Concord::Skeleton& skeleton = scaled.asset.skeletons[0];
    if (!Near(skeleton.joints[0].local.translation.x, 1.0f)) return false;
    if (!Near(skeleton.joints[0].inverseBind.col[3].x, -1.0f)) return false;
    const Concord::AnimationChannel& channel = scaled.asset.animations[0].channels[0];
    return Near(channel.vec3Keys[0].value.x, 2.0f) &&
           Near(channel.vec3Keys[1].value.x, 4.0f);
}

bool TestRigOnlyImport()
{
    const Concord::ModelLoadResult result = Load(R"json({
      "asset": {"version": "2.0"},
      "nodes": [{"name": "Hips", "translation": [0.0, 1.0, 0.0]}],
      "skins": [{"joints": [0]}]
    })json", {});
    if (!result.Succeeded() || !result.asset.IsValid() || !result.asset.meshes.empty() ||
        result.asset.skeletons.size() != 1 ||
        !Near(result.asset.skeletons[0].joints[0].local.translation.y, 1.0f)) {
        std::printf("  rig-only: error='%s' valid=%d meshes=%zu skeletons=%zu nodes=%zu\n",
                    result.error.message.c_str(), result.asset.IsValid() ? 1 : 0,
                    result.asset.meshes.size(), result.asset.skeletons.size(),
                    result.asset.nodes.size());
        return false;
    }
    return true;
}

/** Non-indexed triangle with JOINTS_0 but no WEIGHTS_0. */
const char* kUnpairedJson = R"json({
  "asset": {"version": "2.0"},
  "nodes": [{"mesh": 0}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "JOINTS_0": 1}}]}],
  "materials": [{}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5123, "count": 3, "type": "VEC4"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 24}],
  "buffers": [{"uri": "@DATA@", "byteLength": @LENGTH@}]
})json";

bool TestJointWeightPairing()
{
    std::vector<unsigned char> bytes;
    AppendFloats(bytes, {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f});
    AppendU16(bytes, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const std::string json = WithData(kUnpairedJson, bytes);

    const Concord::ModelLoadResult strict = Load(json, {});
    if (strict.Succeeded() || strict.error.message.find("WEIGHTS_0") == std::string::npos) {
        return false;
    }
    const Concord::ModelLoadResult lenient = Load(json, {.strict = false});
    return lenient.Succeeded() && lenient.asset.IsValid();
}

bool TestNegativeScaleMatrix()
{
    const Concord::ModelLoadResult strict = Load(R"json({
      "asset": {"version": "2.0"},
      "nodes": [{"matrix": [-1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]}],
      "skins": [{"joints": [0]}]
    })json", {});
    if (strict.Succeeded() || strict.error.message.find("negative scale") == std::string::npos) {
        return false;
    }
    const Concord::ModelLoadResult lenient = Load(R"json({
      "asset": {"version": "2.0"},
      "nodes": [{"matrix": [-1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]}],
      "skins": [{"joints": [0]}]
    })json", {.strict = false});
    return lenient.Succeeded() &&
           Near(lenient.asset.nodes[0].local.rotation.w, 1.0f) &&
           Near(lenient.asset.nodes[0].local.scale.x, 1.0f);
}

bool TestUnresolvedAnimationTarget()
{
    const char* json = R"json({
      "asset": {"version": "2.0"},
      "nodes": [{"name": "Solo"}],
      "animations": [{"name": "free",
        "samplers": [{"input": 0, "output": 1}],
        "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]}],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR"},
        {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3"}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 8},
        {"buffer": 0, "byteOffset": 8, "byteLength": 24}],
      "buffers": [{"uri": "@DATA@", "byteLength": @LENGTH@}]
    })json";
    std::vector<unsigned char> bytes;
    AppendFloats(bytes, {0.0f, 1.0f, 1.0f, 2.0f, 3.0f, 0.0f, 4.0f, 5.0f});

    const Concord::ModelLoadResult strict = Load(WithData(json, bytes), {});
    if (strict.Succeeded() ||
        strict.error.message.find("targets no skeleton") == std::string::npos) {
        std::printf("  unresolved-target strict: ok=%d error='%s' line=%zu col=%zu\n",
                    strict.Succeeded() ? 1 : 0, strict.error.message.c_str(),
                    strict.error.line, strict.error.column);
        return false;
    }
    const Concord::ModelLoadResult lenient = Load(WithData(json, bytes), {.strict = false});
    if (!lenient.Succeeded() || lenient.asset.animations.size() != 1) {
        std::printf("  unresolved-target lenient: ok=%d error='%s' animations=%zu\n",
                    lenient.Succeeded() ? 1 : 0, lenient.error.message.c_str(),
                    lenient.asset.animations.size());
        return false;
    }
    return true;
}

} // namespace

int main()
{
    struct Case {
        const char* name;
        bool (*run)();
    };
    const Case cases[] = {
        {"import-scale-consistency", TestImportScaleConsistency},
        {"rig-only-import", TestRigOnlyImport},
        {"joint-weight-pairing", TestJointWeightPairing},
        {"negative-scale-matrix", TestNegativeScaleMatrix},
        {"unresolved-animation-target", TestUnresolvedAnimationTarget},
    };
    for (const Case& testCase : cases) {
        if (!testCase.run()) {
            std::printf("FAIL %s\n", testCase.name);
            return 1;
        }
    }
    return 0;
}
