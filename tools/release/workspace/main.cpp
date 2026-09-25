#include <Concord/CAnimation.h>
#include <Concord/CApplication.h>
#include <Concord/CAsset.h>
#include <Concord/CAudio.h>
#include <Concord/CCamera.h>
#include <Concord/CColor.h>
#include <Concord/CController.h>
#include <Concord/CEcs.h>
#include <Concord/CInput.h>
#include <Concord/CLight.h>
#include <Concord/CObject.h>
#include <Concord/CParticle.h>
#include <Concord/CScene.h>
#include <Concord/CDayCycle.h>
#include <Concord/CPhysics.h>
#include <Concord/CWater.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using Concord::f32;
using Concord::u16;
using Concord::u32;

/**
 * An embedded 64x64 PNG colour chart, used as the demo's base-colour texture.
 *
 * Shipping it inline keeps the demo self-contained and exercises the whole
 * import path -- glTF image URI, data-URI decode, texture cache and the
 * per-material sampler -- without depending on a file next to the executable.
 */
constexpr const char* kDemoTextureUri =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAIUSURBVHhe7dChDQJBFEXRrYGEZCtAINEoxDgMoQ401aEpgg4wdAD+iGmAK4674v2/vI6H78x5u5myl73G/T217i9T9rLX4mA5WPayl4PlYNnLXj3AwXKw7GUvB8vBspe9eoCD5WDZy14OloNlL3v1AAfLwbKXvRwsB8te9locJA+SvezlIHmQ7GWvHuBgOVj2speD5WDZy149wMFysOxlLwfLwbKXvXqAg+Vg2cteDpaDZS979QAHyYNkL3s5SB4ke9mrBzhYDpa97OVgOVj2slcPcLAcLHvZy8FysOxlrx7gYDlY9rKXg+Vg2ctePcBB8iDZy14OkgfJXvbqAQ6Wg2UvezlYDpa97NUDHCwHy172crAcLHvZqwc4WA6WvezlYDlY9rJXDxiP23dmPe2m7GWvz3NMXcc6ZS979QAHy8Gyl70cLAfLXvbqAQ6Wg2UvezlYDpa97NUDHCwHy172crAcLHvZqwc4SB4ke9nLQfIg2ctePcDBcrDsZS8Hy8Gyl716gIPlYNnLXg6Wg2Uve/UAB8vBspe9HCwHy1726gEOkgfJXvZykDxI9rJXD3CwHCx72cvBcrDsZa8e4GA5WPayl4PlYNnLXj3AwXKw7GUvB8vBspe9eoCD5EGyl70cJA+SvezVAxwsB8te9nKwHCx72asHOFgOlr3s5WA5WPayVw9wsBwse9nLwXKw7GWvv3/AD21PK/Db9j6nAAAAAElFTkSuQmCC";

/** Raw float/short payload of the generated glTF buffer. */
struct AssetBlob {
    std::vector<f32> floats;
    std::vector<u16> shorts;
    u32 vertexCount = 0;

    void AddFloats(const f32* values, u32 count)
    {
        floats.insert(floats.end(), values, values + count);
    }

    void AddShorts(const u16* values, u32 count)
    {
        shorts.insert(shorts.end(), values, values + count);
    }
};

std::string ToBase64(const std::vector<unsigned char>& bytes)
{
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    for (u32 index = 0; index < bytes.size(); index += 3) {
        const u32 chunk = (static_cast<u32>(bytes[index]) << 16) |
                          (index + 1 < bytes.size() ? static_cast<u32>(bytes[index + 1]) << 8 : 0) |
                          (index + 2 < bytes.size() ? static_cast<u32>(bytes[index + 2]) : 0);
        out += kTable[(chunk >> 18) & 63];
        out += kTable[(chunk >> 12) & 63];
        out += index + 1 < bytes.size() ? kTable[(chunk >> 6) & 63] : '=';
        out += index + 2 < bytes.size() ? kTable[chunk & 63] : '=';
    }
    return out;
}

std::string ToDataUri(const AssetBlob& blob)
{
    std::vector<unsigned char> bytes;
    bytes.reserve((blob.floats.size() + blob.shorts.size()) * 4);
    for (const f32 value : blob.floats) {
        unsigned char encoded[4];
        std::memcpy(encoded, &value, sizeof(encoded));
        bytes.insert(bytes.end(), encoded, encoded + sizeof(encoded));
    }
    for (const u16 value : blob.shorts) {
        bytes.push_back(static_cast<unsigned char>(value & 0xFFu));
        bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xFFu));
    }
    return "data:application/octet-stream;base64," + ToBase64(bytes);
}

/** Appends one axis-aligned box segment skinned to a single joint. */
void AddSegment(std::vector<f32>& positions, std::vector<f32>& weights,
                std::vector<u16>& indices, std::vector<u16>& joints, u32 joint, f32 x0, f32 x1)
{
    constexpr f32 kHalfHeight = 0.22f;
    constexpr f32 kHalfDepth = 0.22f;
    const f32 corners[8][3] = {
        {x0, -kHalfHeight, -kHalfDepth}, {x1, -kHalfHeight, -kHalfDepth},
        {x1, kHalfHeight, -kHalfDepth},  {x0, kHalfHeight, -kHalfDepth},
        {x0, -kHalfHeight, kHalfDepth},  {x1, -kHalfHeight, kHalfDepth},
        {x1, kHalfHeight, kHalfDepth},   {x0, kHalfHeight, kHalfDepth},
    };
    constexpr u16 kFaces[6][6] = {
        {0, 3, 2, 0, 2, 1}, {4, 5, 6, 4, 6, 7}, {0, 4, 7, 0, 7, 3},
        {1, 2, 6, 1, 6, 5}, {0, 1, 5, 0, 5, 4}, {3, 7, 6, 3, 6, 2},
    };
    const u16 base = static_cast<u16>(positions.size() / 3);
    for (const auto& corner : corners) positions.insert(positions.end(), corner, corner + 3);
    for (const auto& face : kFaces) {
        for (const u16 offset : face) indices.push_back(static_cast<u16>(base + offset));
    }
    for (u32 i = 0; i < 8; ++i) {
        joints.push_back(static_cast<u16>(joint));
        joints.push_back(0);
        joints.push_back(0);
        joints.push_back(0);
        weights.push_back(1.0f);
        weights.push_back(0.0f);
        weights.push_back(0.0f);
        weights.push_back(0.0f);
    }
}

/** Appends a rotation channel's quaternions for a yaw wave of `amplitude`. */
void AddWaveChannel(AssetBlob& blob, const f32* times, u32 count, f32 amplitude, f32 phase)
{
    for (u32 i = 0; i < count; ++i) {
        const f32 radians = amplitude * std::sin(phase + times[i] * 1.6f) * 0.00872664626f;
        const f32 rotation[4] = {0.0f, std::sin(radians), 0.0f, std::cos(radians)};
        blob.AddFloats(rotation, 4);
    }
}

struct GeneratedAsset {
    std::string json;
    bool ok = false;
};

/**
 * Builds a skinned three-segment tail with two wave clips and Mixamo-style
 * joint naming, exercising import, GPU skinning, and the state machine.
 */
GeneratedAsset BuildDemoTailAsset()
{
    AssetBlob blob;

    // Accessor 0: inverse bind matrices for the three joints (pure translations).
    const f32 inverseBinds[48] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -2, 0, 0, 1,
    };
    blob.AddFloats(inverseBinds, 48);

    // Accessors 1-4: three skinned box segments along +X, partitioned so the
    // accessor table's contiguous byte ranges match the generated order.
    std::vector<f32> positions;
    std::vector<f32> weights;
    std::vector<u16> indices;
    std::vector<u16> joints;
    for (u32 segment = 0; segment < 3; ++segment) {
        const f32 x0 = static_cast<f32>(segment);
        AddSegment(positions, weights, indices, joints, segment, x0, x0 + 1.0f);
    }
    blob.AddFloats(positions.data(), static_cast<u32>(positions.size()));
    blob.AddFloats(weights.data(), static_cast<u32>(weights.size()));

    // Wave clips: shared time track per clip, one rotation track per bone.
    const u32 slowCount = 5;
    const f32 slowTimes[5] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    const u32 fastCount = 4;
    const f32 fastTimes[4] = {0.0f, 0.3f, 0.6f, 0.9f};
    blob.AddFloats(slowTimes, slowCount);
    blob.AddFloats(fastTimes, fastCount);
    AddWaveChannel(blob, slowTimes, slowCount, 30.0f, 0.0f);
    AddWaveChannel(blob, slowTimes, slowCount, 24.0f, 0.9f);
    AddWaveChannel(blob, slowTimes, slowCount, 16.0f, 1.8f);
    AddWaveChannel(blob, fastTimes, fastCount, 46.0f, 0.0f);
    AddWaveChannel(blob, fastTimes, fastCount, 34.0f, 1.1f);
    AddWaveChannel(blob, fastTimes, fastCount, 22.0f, 2.2f);

    blob.AddShorts(indices.data(), static_cast<u32>(indices.size()));
    blob.AddShorts(joints.data(), static_cast<u32>(joints.size()));

    const u32 floatBytes = static_cast<u32>(blob.floats.size()) * 4;
    const u32 shortsBytes = static_cast<u32>(blob.shorts.size()) * 2;
    const u32 totalBytes = floatBytes + shortsBytes;
    const std::string uri = ToDataUri(blob);

    std::string json = R"json({
  "asset": {"version": "2.0", "generator": "ConcordFlashDemo"},
  "nodes": [
    {"name": "mixamorig:Armature", "children": [1]},
    {"name": "mixamorig:Bone1", "children": [2], "translation": [0.0, 0.0, 0.0]},
    {"name": "mixamorig:Bone2", "children": [3], "translation": [1.0, 0.0, 0.0]},
    {"name": "mixamorig:Bone3", "translation": [1.0, 0.0, 0.0]},
    {"name": "TailMesh", "mesh": 0, "skin": 0}
  ],
  "skins": [{"joints": [1, 2, 3], "skeleton": 1, "inverseBindMatrices": 0}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 1, "JOINTS_0": 12, "WEIGHTS_0": 2}, "indices": 11, "material": 0}]}],
  "images": [{"uri": "@TEX@"}],
  "textures": [{"source": 0}],
  "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, 1.0], "metallicFactor": 0.05, "roughnessFactor": 0.55, "baseColorTexture": {"index": 0}}}],
  "animations": [
    {"name": "WaveSlow", "samplers": [
        {"input": 3, "output": 5}, {"input": 3, "output": 6}, {"input": 3, "output": 7}],
     "channels": [
        {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
        {"sampler": 1, "target": {"node": 2, "path": "rotation"}},
        {"sampler": 2, "target": {"node": 3, "path": "rotation"}}]},
    {"name": "WaveFast", "samplers": [
        {"input": 4, "output": 8}, {"input": 4, "output": 9}, {"input": 4, "output": 10}],
     "channels": [
        {"sampler": 0, "target": {"node": 1, "path": "rotation"}},
        {"sampler": 1, "target": {"node": 2, "path": "rotation"}},
        {"sampler": 2, "target": {"node": 3, "path": "rotation"}}]}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "MAT4"},
    {"bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 24, "type": "VEC4"},
    {"bufferView": 3, "componentType": 5126, "count": 5, "type": "SCALAR"},
    {"bufferView": 4, "componentType": 5126, "count": 4, "type": "SCALAR"},
    {"bufferView": 5, "componentType": 5126, "count": 5, "type": "VEC4"},
    {"bufferView": 6, "componentType": 5126, "count": 5, "type": "VEC4"},
    {"bufferView": 7, "componentType": 5126, "count": 5, "type": "VEC4"},
    {"bufferView": 8, "componentType": 5126, "count": 4, "type": "VEC4"},
    {"bufferView": 9, "componentType": 5126, "count": 4, "type": "VEC4"},
    {"bufferView": 10, "componentType": 5126, "count": 4, "type": "VEC4"},
    {"bufferView": 11, "componentType": 5123, "count": 108, "type": "SCALAR"},
    {"bufferView": 12, "componentType": 5123, "count": 24, "type": "VEC4"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 192},
    {"buffer": 0, "byteOffset": 192, "byteLength": 288},
    {"buffer": 0, "byteOffset": 480, "byteLength": 384},
    {"buffer": 0, "byteOffset": 864, "byteLength": 20},
    {"buffer": 0, "byteOffset": 884, "byteLength": 16},
    {"buffer": 0, "byteOffset": 900, "byteLength": 80},
    {"buffer": 0, "byteOffset": 980, "byteLength": 80},
    {"buffer": 0, "byteOffset": 1060, "byteLength": 80},
    {"buffer": 0, "byteOffset": 1140, "byteLength": 64},
    {"buffer": 0, "byteOffset": 1204, "byteLength": 64},
    {"buffer": 0, "byteOffset": 1268, "byteLength": 64},
    {"buffer": 0, "byteOffset": 1332, "byteLength": 216},
    {"buffer": 0, "byteOffset": 1548, "byteLength": 192}],
  "buffers": [{"uri": "@URI@", "byteLength": @LENGTH@}]
})json";

    // The JSON above hardcodes this layout; fail loudly if the generator and
    // the accessor table ever drift apart.
    if (floatBytes != 1332 || shortsBytes != 408) {
        return {"", false};
    }
    const std::size_t uriAt = json.find("@URI@");
    if (uriAt == std::string::npos) return {"", false};
    json.replace(uriAt, 5, uri);
    const std::size_t lengthAt = json.find("@LENGTH@");
    if (lengthAt == std::string::npos) return {"", false};
    json.replace(lengthAt, 8, std::to_string(totalBytes));
    const std::size_t textureAt = json.find("@TEX@");
    if (textureAt == std::string::npos) return {"", false};
    json.replace(textureAt, 5, kDemoTextureUri);
    return {json, true};
}

/** Appends a UV-sphere's positions, smooth normals, and triangle indices. */
void AddSphereGeometry(AssetBlob& blob, std::vector<u16>& indices,
                       u32 widthSegments, u32 heightSegments)
{
    constexpr f32 kPi = 3.14159265f;
    constexpr f32 kTwoPi = 6.28318530f;
    std::vector<f32> positions;
    std::vector<f32> normals;
    positions.reserve(static_cast<std::size_t>((widthSegments + 1) * (heightSegments + 1)) * 3);
    normals.reserve(positions.capacity());
    for (u32 y = 0; y <= heightSegments; ++y) {
        const f32 phi = static_cast<f32>(y) / static_cast<f32>(heightSegments) * kPi;
        for (u32 x = 0; x <= widthSegments; ++x) {
            const f32 theta = static_cast<f32>(x) / static_cast<f32>(widthSegments) * kTwoPi;
            const f32 nx = -std::sin(phi) * std::cos(theta);
            const f32 ny = std::cos(phi);
            const f32 nz = std::sin(phi) * std::sin(theta);
            const f32 position[3] = {nx, ny, nz};
            positions.insert(positions.end(), position, position + 3);
            normals.insert(normals.end(), position, position + 3);
        }
    }
    for (u32 y = 0; y < heightSegments; ++y) {
        for (u32 x = 0; x < widthSegments; ++x) {
            const u16 row = static_cast<u16>(y * (widthSegments + 1));
            const u16 a = static_cast<u16>(row + x);
            const u16 b = static_cast<u16>(a + widthSegments + 1);
            const u16 quad[6] = {a, b, static_cast<u16>(a + 1),
                                 b, static_cast<u16>(b + 1), static_cast<u16>(a + 1)};
            indices.insert(indices.end(), quad, quad + 6);
        }
    }
    blob.AddFloats(positions.data(), static_cast<u32>(positions.size()));
    blob.AddFloats(normals.data(), static_cast<u32>(normals.size()));
}

/**
 * Builds a one-metre mirror sphere (metallic 1.0, roughness 0.05) whose
 * reflections the ray tracer resolves with a real reflection ray.
 */
GeneratedAsset BuildDemoSphereAsset()
{
    AssetBlob blob;
    std::vector<u16> indices;
    constexpr u32 kWidthSegments = 40;
    constexpr u32 kHeightSegments = 28;
    AddSphereGeometry(blob, indices, kWidthSegments, kHeightSegments);
    blob.AddShorts(indices.data(), static_cast<u32>(indices.size()));

    const u32 vertexCount = (kWidthSegments + 1) * (kHeightSegments + 1);
    const u32 triangleCount = kWidthSegments * kHeightSegments * 2;
    const u32 positionsBytes = vertexCount * 12;
    const u32 totalBytes = vertexCount * 24 + static_cast<u32>(indices.size()) * 2;
    const std::string uri = ToDataUri(blob);

    std::string json = R"json({
  "asset": {"version": "2.0", "generator": "ConcordFlashDemo"},
  "nodes": [{"name": "MirrorSphere", "mesh": 0}],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]}],
  "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [0.92, 0.95, 1.0, 1.0],
                  "metallicFactor": 1.0, "roughnessFactor": 0.05}}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": @VERTS@, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5126, "count": @VERTS@, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5123, "count": @TRIS3@, "type": "SCALAR"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": @POS@},
    {"buffer": 0, "byteOffset": @POS@, "byteLength": @POS@},
    {"buffer": 0, "byteOffset": @IDXF@, "byteLength": @IDX@}],
  "buffers": [{"uri": "@URI@", "byteLength": @LENGTH@}]
})json";

    if (blob.floats.size() != static_cast<std::size_t>(vertexCount) * 6 ||
        indices.size() != static_cast<std::size_t>(triangleCount) * 3) {
        return {"", false};
    }
    const std::string positionsBytesText = std::to_string(positionsBytes);
    struct Replacement {
        const char* token;
        std::string value;
    };
    const Replacement replacements[] = {
        {"@VERTS@", std::to_string(vertexCount)},
        {"@TRIS3@", std::to_string(triangleCount * 3)},
        {"@POS@", positionsBytesText},
        {"@IDXF@", std::to_string(positionsBytes * 2)},
        {"@IDX@", std::to_string(static_cast<u32>(indices.size()) * 2)},
        {"@LENGTH@", std::to_string(totalBytes)},
    };
    std::size_t uriAt = json.find("@URI@");
    if (uriAt == std::string::npos) return {"", false};
    json.replace(uriAt, 5, uri);
    for (const Replacement& replacement : replacements) {
        // Tokens may repeat (both bufferViews share @POS@); replace every one.
        for (std::size_t at = json.find(replacement.token); at != std::string::npos;
             at = json.find(replacement.token, at + replacement.value.size())) {
            json.replace(at, std::strlen(replacement.token), replacement.value);
        }
        if (json.find(replacement.token) != std::string::npos) return {"", false};
    }
    return {json, true};
}

/** Swaps the tail between its two wave states every few seconds. */
class TailDemoSystem final : public Concord::ISystem {
public:
    explicit TailDemoSystem(const Concord::AnimationGraph& graph) : m_graph(graph) {}

    void OnUpdate(Concord::Scene& scene, f32 deltaTime) override
    {
        m_timer += deltaTime;
        if (m_timer < 4.0f) return;
        m_timer = 0.0f;
        m_fast = !m_fast;
        scene.Query<Concord::AnimationControllerComponent>(
            [&](Concord::Entity, Concord::AnimationControllerComponent& controller) {
                Concord::RequestAnimationTransition(
                    m_graph, m_fast ? "WaveFast" : "WaveSlow", controller.runtime);
            });
    }

private:
    const Concord::AnimationGraph& m_graph;
    f32 m_timer = 3.0f;
    bool m_fast = false;
};

} // namespace

/**
 * Builds a flat quad whose material the caller flags as water.
 *
 * Two triangles are enough: the surface is shaded rather than displaced, so
 * the wave detail comes from the hit position and a denser grid would only
 * add vertices that never move.
 */
GeneratedAsset BuildDemoWaterAsset(f32 halfSize)
{
    AssetBlob blob;
    const f32 positions[4][3] = {{-halfSize, 0.0f, -halfSize},
                                 {halfSize, 0.0f, -halfSize},
                                 {halfSize, 0.0f, halfSize},
                                 {-halfSize, 0.0f, halfSize}};
    for (const auto& corner : positions) {
        blob.AddFloats(corner, 3);
    }
    const f32 up[3] = {0.0f, 1.0f, 0.0f};
    for (u32 index = 0; index < 4; ++index) {
        blob.AddFloats(up, 3);
    }
    const u16 indices[6] = {0, 2, 1, 0, 3, 2};
    blob.AddShorts(indices, 6);

    const u32 totalBytes = 4 * 12 * 2 + 6 * 2;
    const std::string uri = ToDataUri(blob);

    std::string json = R"json({
  "asset": {"version": "2.0", "generator": "ConcordFlashDemo"},
  "nodes": [{"name": "Water", "mesh": 0}],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0}]}],
  "materials": [{"name": "Water", "pbrMetallicRoughness": {"baseColorFactor": [0.02, 0.115, 0.155, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.04}}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48},
    {"buffer": 0, "byteOffset": 48, "byteLength": 48},
    {"buffer": 0, "byteOffset": 96, "byteLength": 12}],
  "buffers": [{"uri": "@URI@", "byteLength": @LENGTH@}]
})json";

    const std::size_t uriAt = json.find("@URI@");
    if (uriAt == std::string::npos) return {"", false};
    json.replace(uriAt, 5, uri);
    const std::size_t lengthAt = json.find("@LENGTH@");
    if (lengthAt == std::string::npos) return {"", false};
    json.replace(lengthAt, 8, std::to_string(totalBytes));
    return {json, true};
}

/**
 * A vertical sheet in the XY plane, for a waterfall.
 *
 * Centered on the origin so the spawn transform places the middle of the
 * column. The +Z normal faces the default camera.
 */
GeneratedAsset BuildDemoSheetAsset(f32 halfWidth, f32 halfHeight)
{
    AssetBlob blob;
    const f32 positions[4][3] = {{-halfWidth, -halfHeight, 0.0f},
                                 {halfWidth, -halfHeight, 0.0f},
                                 {halfWidth, halfHeight, 0.0f},
                                 {-halfWidth, halfHeight, 0.0f}};
    for (const auto& corner : positions) {
        blob.AddFloats(corner, 3);
    }
    const f32 facing[3] = {0.0f, 0.0f, 1.0f};
    for (u32 index = 0; index < 4; ++index) {
        blob.AddFloats(facing, 3);
    }
    // u across, v up: the hit shader panners v so the sheet flows down.
    const f32 uvs[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    for (const auto& corner : uvs) {
        blob.AddFloats(corner, 2);
    }
    const u16 indices[6] = {0, 1, 2, 0, 2, 3};
    blob.AddShorts(indices, 6);

    const u32 totalBytes = 4 * 12 * 2 + 4 * 8 + 6 * 2;
    const std::string uri = ToDataUri(blob);

    std::string json = R"json({
  "asset": {"version": "2.0", "generator": "ConcordFlashDemo"},
  "nodes": [{"name": "Waterfall", "mesh": 0}],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0}]}],
  "materials": [{"name": "Waterfall", "pbrMetallicRoughness": {"baseColorFactor": [0.12, 0.22, 0.28, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.08}}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
    {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48},
    {"buffer": 0, "byteOffset": 48, "byteLength": 48},
    {"buffer": 0, "byteOffset": 96, "byteLength": 32},
    {"buffer": 0, "byteOffset": 128, "byteLength": 12}],
  "buffers": [{"uri": "@URI@", "byteLength": @LENGTH@}]
})json";

    const std::size_t uriAt = json.find("@URI@");
    if (uriAt == std::string::npos) return {"", false};
    json.replace(uriAt, 5, uri);
    const std::size_t lengthAt = json.find("@LENGTH@");
    if (lengthAt == std::string::npos) return {"", false};
    json.replace(lengthAt, 8, std::to_string(totalBytes));
    return {json, true};
}

/**
 * Everything the command line can say about how this run should behave.
 *
 * The demo exists to be looked at, and looking at it is easier from a file
 * than from a window: a still can be rendered above the size of the monitor,
 * has nothing of the desktop in it, and can be produced by a script that has
 * no way to click anything.
 */
struct DemoOptions {
    /** Where to write a single frame, or empty to open a window and run. */
    std::string stillPath;
    u32 width = 1600;
    u32 height = 900;
    /** Simulated frames before the still is taken. */
    u32 warmupFrames = 90;
    /** Clock the still is taken at, so a scene can be photographed at dusk. */
    f32 hour = -1.0f;
    bool overlay = true;
    /**
     * Cloud overrides, each negative when the scene's own value stands.
     *
     * Tuning a volumetric layer is a search rather than a calculation: every
     * control interacts with every other through an integral, and the only way
     * to find out what a value does is to look at it. Driving that search
     * through a rebuild makes each answer cost a minute, which is the
     * difference between converging on a sky and settling for one.
     */
    f32 cloudCoverage = -1.0f;
    f32 cloudDensity = -1.0f;
    f32 cloudLightGain = -1.0f;
    f32 cloudAmbientGain = -1.0f;
    f32 cloudSteps = -1.0f;
    f32 cloudLightSteps = -1.0f;
    f32 exposure = -1.0f;
    /**
     * Degrees to tilt the camera up from where the scene aimed it.
     *
     * The demo's camera looks along the water on purpose, which is the right
     * angle for judging a surface and the worst one for judging a sky: a deck
     * of cloud seen edge-on is a strip at the top of the frame, compressed and
     * seen through the most air there is. Being able to look up is what makes
     * the layer reviewable at all.
     */
    f32 cameraPitchDegrees = 0.0f;
};

DemoOptions ParseDemoOptions(int argc, char** argv)
{
    DemoOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const bool hasValue = index + 1 < argc;
        if (argument == "--still" && hasValue) {
            options.stillPath = argv[++index];
            // A still is an image of the scene; the frame counter drawn over
            // the top of it is the one thing in the frame that is not.
            options.overlay = false;
        } else if (argument == "--width" && hasValue) {
            options.width = static_cast<u32>(std::strtoul(argv[++index], nullptr, 10));
        } else if (argument == "--height" && hasValue) {
            options.height = static_cast<u32>(std::strtoul(argv[++index], nullptr, 10));
        } else if (argument == "--warmup" && hasValue) {
            options.warmupFrames = static_cast<u32>(std::strtoul(argv[++index], nullptr, 10));
        } else if (argument == "--hour" && hasValue) {
            options.hour = std::strtof(argv[++index], nullptr);
        } else if (argument == "--overlay") {
            options.overlay = true;
        } else if (argument == "--cloud-coverage" && hasValue) {
            options.cloudCoverage = std::strtof(argv[++index], nullptr);
        } else if (argument == "--cloud-density" && hasValue) {
            options.cloudDensity = std::strtof(argv[++index], nullptr);
        } else if (argument == "--cloud-gain" && hasValue) {
            options.cloudLightGain = std::strtof(argv[++index], nullptr);
        } else if (argument == "--cloud-ambient" && hasValue) {
            options.cloudAmbientGain = std::strtof(argv[++index], nullptr);
        } else if (argument == "--cloud-steps" && hasValue) {
            options.cloudSteps = std::strtof(argv[++index], nullptr);
        } else if (argument == "--cloud-lightsteps" && hasValue) {
            options.cloudLightSteps = std::strtof(argv[++index], nullptr);
        } else if (argument == "--exposure" && hasValue) {
            options.exposure = std::strtof(argv[++index], nullptr);
        } else if (argument == "--pitch" && hasValue) {
            options.cameraPitchDegrees = std::strtof(argv[++index], nullptr);
        } else {
            std::fprintf(stderr,
                         "[demo] usage: main [--still <out.png> [--width W] [--height H]\n"
                         "                    [--warmup N] [--hour H] [--overlay]]\n"
                         "                   [--cloud-coverage C] [--cloud-density D]\n"
                         "                   [--cloud-gain G] [--cloud-ambient A]\n"
                         "                   [--cloud-steps N] [--cloud-lightsteps N]\n"
                         "                   [--exposure E] [--pitch DEGREES]\n");
        }
    }
    return options;
}

int main(int argc, char** argv)
{
    const DemoOptions options = ParseDemoOptions(argc, argv);
    if (!options.stillPath.empty()) {
        // The render scale exists to buy frame time, and a still has no frame
        // time to buy: the point of rendering one offline is to get the picture
        // the scene actually describes, upscaled from nothing. Left alone when
        // the caller set it, so a deliberate low-resolution still is still
        // possible.
        const char* authored = std::getenv("CONCORD_RENDER_SCALE");
        if (authored == nullptr || *authored == '\0') {
            _putenv_s("CONCORD_RENDER_SCALE", "1");
        }
    }
    const GeneratedAsset generated = BuildDemoTailAsset();
    if (!generated.ok) {
        std::fprintf(stderr, "[demo] failed to build the demo tail asset\n");
        return 1;
    }
    std::filesystem::create_directories("Assets");
    const std::filesystem::path assetPath = "Assets/DemoTail.gltf";
    {
        std::ofstream file(assetPath, std::ios::binary);
        file.write(generated.json.data(), static_cast<std::streamsize>(generated.json.size()));
    }

    const Concord::ModelLoadResult loaded =
        Concord::ModelLoader::Load(assetPath, {.generateNormals = true, .scale = 1.0f});
    if (!loaded.Succeeded()) {
        std::fprintf(stderr, "[demo] tail asset failed to load: %s\n",
                     loaded.error.message.c_str());
        return 1;
    }
    auto tailAsset = std::make_shared<Concord::ModelAsset>(std::move(loaded.asset));
    const Concord::HumanoidSkeleton humanoid =
        Concord::BuildHumanoidSkeleton(tailAsset->skeletons[0]);
    std::printf("[demo] tail asset: joints=%zu clips=%zu humanoid=%s\n",
                tailAsset->skeletons[0].joints.size(), tailAsset->animations.size(),
                humanoid.IsValid() ? "recognized" : "not recognized");

    const GeneratedAsset sphereGenerated = BuildDemoSphereAsset();
    if (!sphereGenerated.ok) {
        std::fprintf(stderr, "[demo] failed to build the demo sphere asset\n");
        return 1;
    }
    const std::filesystem::path spherePath = "Assets/DemoSphere.gltf";
    {
        std::ofstream file(spherePath, std::ios::binary);
        file.write(sphereGenerated.json.data(),
                   static_cast<std::streamsize>(sphereGenerated.json.size()));
    }
    const Concord::ModelLoadResult sphereLoaded =
        Concord::ModelLoader::Load(spherePath, {.generateNormals = false, .scale = 1.0f});
    if (!sphereLoaded.Succeeded()) {
        std::fprintf(stderr, "[demo] sphere asset failed to load: %s\n",
                     sphereLoaded.error.message.c_str());
        return 1;
    }
    auto sphereAsset =
        std::make_shared<Concord::ModelAsset>(std::move(sphereLoaded.asset));
    std::printf("[demo] mirror sphere: %zu triangles\n",
                sphereAsset->meshes[0].primitives[0].indices.size() / 3);

    const GeneratedAsset waterGenerated = BuildDemoWaterAsset(12.5f);
    if (!waterGenerated.ok) {
        std::fprintf(stderr, "[demo] failed to build the water asset\n");
        return 1;
    }
    const std::filesystem::path waterPath = "Assets/DemoWater.gltf";
    {
        std::ofstream file(waterPath, std::ios::binary);
        file.write(waterGenerated.json.data(),
                   static_cast<std::streamsize>(waterGenerated.json.size()));
    }
    const Concord::ModelLoadResult waterLoaded =
        Concord::ModelLoader::Load(waterPath, {.generateNormals = false, .scale = 1.0f});
    if (!waterLoaded.Succeeded()) {
        std::fprintf(stderr, "[demo] water asset failed to load: %s\n",
                     waterLoaded.error.message.c_str());
        return 1;
    }
    auto waterAsset = std::make_shared<Concord::ModelAsset>(std::move(waterLoaded.asset));
    const GeneratedAsset sheetGenerated = BuildDemoSheetAsset(1.35f, 2.85f);
    if (!sheetGenerated.ok) {
        std::fprintf(stderr, "[demo] failed to build the waterfall sheet\n");
        return 1;
    }
    const std::filesystem::path sheetPath = "Assets/DemoWaterfall.gltf";
    {
        std::ofstream file(sheetPath, std::ios::binary);
        file.write(sheetGenerated.json.data(),
                   static_cast<std::streamsize>(sheetGenerated.json.size()));
    }
    const Concord::ModelLoadResult sheetLoaded =
        Concord::ModelLoader::Load(sheetPath, {.generateNormals = false, .scale = 1.0f});
    if (!sheetLoaded.Succeeded()) {
        std::fprintf(stderr, "[demo] waterfall sheet failed to load: %s\n",
                     sheetLoaded.error.message.c_str());
        return 1;
    }
    auto sheetAsset = std::make_shared<Concord::ModelAsset>(std::move(sheetLoaded.asset));
    // glTF has no field for a water surface, so the demo authors one after the
    // import. Every value below is a control the hit shader reads: the sea
    // state, which way the train points, what crosses the surface and how fast
    // the colour falls away with depth.
    // The basin starts from the engine's still-water preset and is nudged
    // further still. A pool in a courtyard is not a wave machine: the whole
    // read of it is a mirror that is never quite flat, so every control here
    // is spent on keeping the reflection intact rather than on making waves.
    Concord::WaterMaterial waterSurface = Concord::MakeStillWater();
    waterSurface.waveAmplitude = 0.14f;
    waterSurface.waveScale = 1.9f;
    waterSurface.waveSpeed = 0.30f;
    waterSurface.choppiness = 0.24f;
    waterSurface.heading = {0.82f, 0.57f};
    waterSurface.spread = 1.15f;
    waterSurface.drift = 0.008f;
    // A dielectric passes nearly everything that is not reflected, so this
    // sits just under one. Lower values read as a tinted film over the water
    // rather than as water, and they hide the bottom for no physical reason.
    waterSurface.opacity = 0.94f;
    waterSurface.ior = 1.333f;
    waterSurface.refraction = 1.0f;
    // Pure water's absorption per metre, averaged over what each channel
    // actually covers rather than sampled at its most extreme wavelength.
    // Red runs from about 0.24 per metre at 600nm to 0.6 at 700nm, and taking
    // the far end makes a two-metre basin absorb like a ten-metre one: the
    // bottom loses 70% of its red and everything reads as a teal filter.
    // Averaged, red is absorbed roughly sixteen times faster than blue, which
    // is still what makes deep water blue and a shallow bottom keep its colour.
    waterSurface.absorption = {0.34f, 0.075f, 0.020f};
    waterSurface.absorptionDistance = 1.0f;
    // Raised now that the in-scattering term is no longer carrying a stray
    // 1/4pi that made it about thirty times too small to see. Distilled water
    // really does scatter almost nothing, and a basin rendered that way is
    // optically correct and reads as an empty hole: what a pool in daylight
    // actually shows is the light the volume sends back, and without it the
    // only evidence of water is that the bottom looks slightly bent.
    //
    // Still an order of magnitude under the absorption, which is the bound
    // that keeps it water: once scattering rivals extinction the colour stops
    // depending on depth and the basin turns to milk.
    waterSurface.scattering = 0.022f;
    waterSurface.foamThreshold = 0.82f;
    waterSurface.foamIntensity = 0.10f;
    // Two small, local disturbances rather than two ring machines. The reach is
    // what makes them local: at a few wavelengths a ring is something that
    // touched the water nearby and is over in seconds, where a reach the size
    // of the basin turns one drop into a wave that crosses all of it.
    waterSurface.ripples[0] = {.centre = {2.7f, 1.8f},
                               .wavelength = 1.40f,
                               .speed = 0.35f,
                               .strength = 0.022f,
                               .falloff = 0.55f,
                               .reach = 3.0f};
    waterSurface.ripples[1] = {.centre = {-4.4f, -2.6f},
                               .wavelength = 1.00f,
                               .speed = 0.26f,
                               .strength = 0.017f,
                               .falloff = 0.65f,
                               .reach = 2.2f};
    if (!Concord::SanitizeWaterMaterial(waterSurface)) {
        std::fprintf(stderr, "[demo] water surface had out-of-range fields; clamped\n");
    }

    Concord::AnimationGraph graph;
    graph.states.push_back({.name = "WaveSlow", .clipIndex = 0});
    graph.states.push_back({.name = "WaveFast", .clipIndex = 1});
    graph.transitions.push_back({.fromState = 0, .toState = 1, .duration = 0.8f});
    graph.transitions.push_back({.fromState = 1, .toState = 0, .duration = 0.8f});
    graph.initialState = 0;

    const bool stillMode = !options.stillPath.empty();
    Concord::Game game;
    Concord::Window window(
        {.title = "Concord Flash Demo - WASD move | click: mouse look | Esc: release | "
                  "Space/Ctrl: up/down | Shift: sprint",
         .resolution = {.width = options.width, .height = options.height},
         // Nothing is presented in still mode, so nothing should appear. The
         // window still exists because the device is reached through one; it
         // is simply never mapped onto the desktop.
         .visible = !stillMode,
         // Mailbox rather than FIFO: the trace runs at its own rate, and a
         // vsync-locked swapchain turns every rate below the refresh into a
         // stutter rhythm. Latest-frame presentation stays smooth instead.
         .vsync = false});
    game.AttachWindow(window);
    game.Overlay().showDebugInfo = options.overlay;

    Concord::Scene scene;
    // Low and close to the surface, on a longer lens. A high wide-angle view
    // reads the water as a lid; this one looks along it, where the grazing
    // reflection lives and the refraction has depth to reach through.
    // High enough to look down into the water rather than across it. Fresnel
    // is the whole story here: at a grazing angle most of what returns is a
    // reflection of the sky, and the bottom is only visible when the view meets
    // the surface steeply enough that light can cross it.
    const Concord::Vec3 cameraEye{-1.1f, 3.15f, -7.6f};
    Concord::Vec3 cameraTarget{-1.0f, 0.6f, 3.2f};
    if (options.cameraPitchDegrees != 0.0f) {
        // Rotated about the horizontal axis perpendicular to the view, so the
        // bearing is preserved and only the elevation changes.
        const Concord::Vec3 forward{cameraTarget.x - cameraEye.x, cameraTarget.y - cameraEye.y,
                                    cameraTarget.z - cameraEye.z};
        const f32 groundLength = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        const f32 pitch = std::atan2(forward.y, groundLength) +
                          options.cameraPitchDegrees * 3.14159265f / 180.0f;
        const f32 reach = std::sqrt(forward.x * forward.x + forward.y * forward.y +
                                    forward.z * forward.z);
        const f32 scale = groundLength > 0.0001f ? reach * std::cos(pitch) / groundLength : 0.0f;
        cameraTarget = {cameraEye.x + forward.x * scale, cameraEye.y + reach * std::sin(pitch),
                        cameraEye.z + forward.z * scale};
    }
    Concord::EntityHandle camera = scene.Spawn<Concord::Object::Camera>(
        {.position = cameraEye, .target = cameraTarget, .fovYDegrees = 54.0f});
    camera.Add<Concord::AudioListener>({});
    // A low sun is what puts a long specular streak down a water surface; at
    // noon the same surface is a flat white sheen.
    scene.Spawn<Concord::Object::SunLight>(
        {.elevationDegrees = 31.0f, .azimuthDegrees = 206.0f, .intensity = 6.5f});

    // A basin deep enough to look into. Water needs distance for the light to
    // travel: over a puddle the refraction has nothing to absorb through and
    // reads as a pane of glass laid on the floor, which is what a few
    // centimetres of it amounts to.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {0.0f, -2.6f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {30.0f, 1.2f, 30.0f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(108, 106, 100), .roughness = 0.95f});
    // On the basin floor, two metres down: these are what the surface is a
    // window onto, and what the extinction gradient is measured against.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {-3.2f, -1.78f, -1.0f}, .rotation = {0.0f, 18.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {2.4f, 0.5f, 1.6f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(196, 132, 72), .roughness = 0.62f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {2.6f, -1.62f, 0.6f}, .rotation = {0.0f, -34.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {1.8f, 0.9f, 1.8f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(120, 158, 172), .roughness = 0.45f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {0.4f, -1.92f, 2.4f}, .rotation = {0.0f, 42.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {3.0f, 0.3f, 1.2f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(92, 96, 108), .metallic = 0.35f, .roughness = 0.3f});
    // Broad, strongly coloured plates across the basin floor. A body of water
    // is only ever seen through, and what it is seen through decides what it
    // reads as: over one grey slab the same absorption curve looks like a
    // filter laid on the frame, while over shapes with colour of their own it
    // reads as depth. These also make the refraction legible, because their
    // straight edges bend visibly under a moving surface.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {0.0f, -1.98f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {9.0f, 0.2f, 9.0f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(216, 186, 138), .roughness = 0.85f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {-5.4f, -1.88f, 3.2f}, .rotation = {0.0f, -12.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {4.5f, 0.25f, 4.5f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(42, 92, 138), .roughness = 0.7f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {5.8f, -1.82f, -2.4f}, .rotation = {0.0f, 26.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {4.0f, 0.3f, 4.0f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(168, 62, 48), .roughness = 0.75f});

    // Standing through the waterline, so each is both mirrored in the surface
    // above it and seen through the surface in front of it.
    // The three pillars are the demo's material chart, not decoration. Now that
    // the specular lobe is a real GGX microfacet term rather than a Blinn-Phong
    // exponent, metalness and roughness finally mean what they say, and the
    // only way to show that is to stand three surfaces next to each other that
    // differ in nothing else: a painted dielectric, a brushed metal and a
    // polished one. The difference between them is the whole point of a
    // physically based shading model, and a scene where everything is matte
    // demonstrates none of it.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {-4.4f, -0.1f, -2.6f}, .rotation = {0.0f, 25.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {1.4f, 4.4f, 1.4f}})
        // Painted stone: a dielectric, so its highlight stays white and narrow
        // however red the body under it is.
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(198, 74, 62), .metallic = 0.0f, .roughness = 0.34f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {4.6f, 0.3f, -3.4f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {1.2f, 5.2f, 1.2f}})
        // Brushed metal: conductive, so it has almost no diffuse and the sky it
        // reflects is tinted by its own albedo.
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(126, 158, 208), .metallic = 0.88f, .roughness = 0.30f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{
            .position = {1.6f, 0.35f, 4.2f}, .rotation = {0.0f, 45.0f, 0.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {1.8f, 1.4f, 1.8f}})
        // Polished brass: the same conductor at half the roughness, which is
        // the difference between reflecting the sky and reflecting the scene.
        //
        // Not taken all the way to a mirror. A perfect conductor has no diffuse
        // to fall back on, so it is worth exactly what it can see -- and half
        // of what anything in this courtyard can see is unlit ground, which
        // renders a polished block as a black one. That is correct, and it is
        // also the reason a real product shot puts a card where the floor is.
        // Until there is something to reflect downward, the roughness is what
        // keeps the block integrating enough sky to read as metal.
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(226, 176, 92), .metallic = 0.92f, .roughness = 0.17f});
    // Ledge and back wall the courtyard fall spills from, so the sheet
    // reads as water leaving stone rather than as a card floating in air.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {-2.85f, 2.65f, 3.05f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {3.2f, 5.5f, 0.70f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(58, 54, 48), .roughness = 0.94f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {-2.85f, 5.55f, 2.40f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {2.8f, 0.50f, 1.55f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(66, 62, 56), .roughness = 0.90f});

    scene.Spawn<Concord::Object::Model>(
        {.asset = tailAsset,
         .transform = {.position = {-0.6f, 1.5f, -0.8f}, .rotation = {0.0f, 90.0f, 0.0f}}});
    // Straddling the waterline: the half above mirrors into the surface, the
    // half below is what the refracted ray finds when it looks through.
    scene.Spawn<Concord::Object::Model>(
        {.asset = sphereAsset,
         .transform = {.position = {2.7f, -0.35f, 1.8f}, .scale = {1.2f, 1.2f, 1.2f}}});
    // castShadow stays off: with a low sun a surface spanning the whole basin
    // would otherwise put every point under it into shadow, and the floor the
    // water is supposed to be a window onto would receive ambient light alone.
    scene.Spawn<Concord::Object::Water>(
        {.asset = waterAsset,
         .surface = waterSurface,
         .transform = {.position = {0.0f, 0.0f, 0.0f}},
         .castShadow = false,
         // Matches the mesh's own half-size, so WaterSplashSystem sees the
         // basin's true footprint rather than a hand-tuned guess.
         .splashExtent = {12.5f, 12.5f}});
    auto oceanAsset = std::make_shared<Concord::ModelAsset>(*waterAsset);
    // Past the courtyard slab, not through it. The basin is 25 units across
    // and ends at z=12.5; the previous ocean at z=42 overlapped that lid and
    // the two surfaces z-fought. A drop and a seawall keep them apart.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {0.0f, -0.55f, 14.4f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {32.0f, 2.6f, 1.1f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(72, 68, 60), .roughness = 0.94f});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {0.0f, -3.4f, 56.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {92.0f, 2.4f, 82.0f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(48, 62, 78), .roughness = 0.96f});
    scene.Spawn<Concord::Object::Ocean>(
        {.asset = oceanAsset,
         .transform = {.position = {0.0f, -0.85f, 56.0f}, .scale = {7.0f, 1.0f, 3.2f}},
         .castShadow = false,
         .splashExtent = {12.5f, 12.5f}});
    auto riverAsset = std::make_shared<Concord::ModelAsset>(*waterAsset);
    // Beside the basin, not on it. Half-width 1.4 at x=16.2 sits past the
    // courtyard water's x=12.5 edge; stacking two water materials in the
    // same cells is what made the river a hole in the pool.
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {16.2f, -0.42f, 1.0f}})
        .Add<Concord::MeshRenderer>(Concord::MeshRenderer{.size = {3.4f, 0.55f, 24.0f}})
        .Add<Concord::Material>(Concord::Material{
            .albedo = COLOR_RGB(64, 58, 50), .roughness = 0.92f});
    scene.Spawn<Concord::Object::River>(
        {.asset = riverAsset,
         .transform = {.position = {16.2f, 0.04f, 1.0f}, .scale = {0.11f, 1.0f, 0.95f}},
         .castShadow = false,
         .splashExtent = {12.5f, 12.5f}});
    const Concord::Vec3 fallCentre{-2.85f, 2.85f, 2.15f};
    // Two overlapping cards, the UE waterfall recipe: one sheet never has
    // enough depth, and four hard slats read as architecture. Offset in Z
    // and a slight scale difference is what turns a card into a volume.
    auto fallFront = std::make_shared<Concord::ModelAsset>(*sheetAsset);
    scene.Spawn<Concord::Object::Waterfall>(
        {.asset = fallFront,
         .transform = {.position = fallCentre},
         .castShadow = false});
    auto fallBack = std::make_shared<Concord::ModelAsset>(*sheetAsset);
    scene.Spawn<Concord::Object::Waterfall>(
        {.asset = fallBack,
         .transform = {.position = {fallCentre.x + 0.10f, fallCentre.y, fallCentre.z - 0.14f},
                       .scale = {0.88f, 1.04f, 1.0f}},
         .castShadow = false});
    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {.position = {fallCentre.x, fallCentre.y + 2.55f, fallCentre.z}},
         .settings = {.shape = Concord::ParticleShape::Box,
                      .shapeExtent = {0.95f, 0.06f, 0.16f},
                      .emissionRate = 720.0f,
                      .lifetime = {0.50f, 0.90f},
                      .speed = {4.4f, 7.2f},
                      .startSize = {0.05f, 0.10f},
                      .endSize = {0.03f, 0.07f},
                      .direction = {0.0f, -1.0f, 0.0f},
                      .spreadDegrees = 7.0f,
                      .gravity = {0.0f, -11.0f, 0.0f},
                      .drag = 0.05f,
                      .startColor = COLOR_RGBA(230, 238, 244, 190),
                      .endColor = COLOR_RGBA(200, 214, 224, 0),
                      .localSpace = false,
                      .capacity = 1024,
                      .blend = Concord::ParticleBlendMode::Additive}});
    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {.position = {fallCentre.x, fallCentre.y, fallCentre.z}},
         .settings = {.shape = Concord::ParticleShape::Box,
                      .shapeExtent = {0.85f, 1.8f, 0.18f},
                      .emissionRate = 220.0f,
                      .lifetime = {0.45f, 0.85f},
                      .speed = {0.15f, 0.45f},
                      .startSize = {0.22f, 0.40f},
                      .endSize = {0.55f, 0.90f},
                      .direction = {0.0f, -1.0f, 0.05f},
                      .spreadDegrees = 12.0f,
                      .gravity = {0.0f, -1.6f, 0.0f},
                      .drag = 0.85f,
                      .startColor = COLOR_RGBA(208, 218, 226, 70),
                      .endColor = COLOR_RGBA(176, 186, 196, 0),
                      .localSpace = false,
                      .capacity = 384,
                      .blend = Concord::ParticleBlendMode::Scatter}});
    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {.position = {fallCentre.x, 0.18f, fallCentre.z - 0.15f}},
         .settings = {.shape = Concord::ParticleShape::Cone,
                      .shapeExtent = {0.75f, 0.55f, 0.0f},
                      .emissionRate = 640.0f,
                      .lifetime = {0.50f, 1.05f},
                      .speed = {2.0f, 4.2f},
                      .startSize = {0.22f, 0.40f},
                      .endSize = {0.55f, 0.95f},
                      .direction = {0.0f, 1.0f, -0.15f},
                      .spreadDegrees = 52.0f,
                      .gravity = {0.0f, -3.6f, 0.0f},
                      .drag = 0.40f,
                      .startColor = COLOR_RGBA(210, 222, 232, 190),
                      .endColor = COLOR_RGBA(176, 188, 200, 0),
                      .localSpace = false,
                      .capacity = 640,
                      .blend = Concord::ParticleBlendMode::Scatter}});
    scene.Spawn<Concord::Object::Sound>({
        .transform = {.position = fallCentre},
        // Built-in preset: layered rumble, hiss and droplets, seamless loop.
        .clip = Concord::AudioClip::WaterfallLoop(),
        .volume = 0.55f,
        .minDistance = 2.0f,
        .maxDistance = 28.0f,
        .spatial = true,
        .loop = true,
    });
    // One small disturbance every few seconds. A field is the control that
    // decides whether a body of water reads as calm or as a downpour, and the
    // whole point of this basin is that it is calm: the rings are here to keep
    // the surface alive, not to be watched. Each is smaller than a stride and
    // gone in three seconds.
    scene.Spawn<Concord::Object::WaterRippleField>(
        {.settings = {.rate = 0.22f,
                      .extent = {9.5f, 9.5f},
                      .wavelength = {0.60f, 1.50f},
                      // A wavefront crosses a fraction of a metre a second, not
                      // a few metres. Faster than that and a ring is gone before
                      // the eye can follow it, which reads as shimmer rather
                      // than as water being disturbed.
                      .speed = {0.18f, 0.40f},
                      .strength = {0.006f, 0.016f},
                      // Crest width as a multiple of the wavelength.
                      .falloff = 0.60f,
                      .reach = 2.2f,
                      .lifetime = {2.0f, 3.6f}}});
    scene.CreateEntity()
        .Add<Concord::AnimationControllerComponent>(Concord::AnimationControllerComponent{
            .asset = tailAsset.get(), .skeletonIndex = 0, .graph = &graph})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{});

    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {.position = {-3.0f, 1.1f, 2.6f}},
         .settings = {.shape = Concord::ParticleShape::Cone,
                      .shapeExtent = {0.4f, 0.15f, 0.0f},
                      .emissionRate = 320.0f,
                      .lifetime = {1.1f, 1.9f},
                      .speed = {1.4f, 3.2f},
                      .startSize = {0.18f, 0.34f},
                      .endSize = {0.02f, 0.07f},
                      .direction = {0.0f, 1.0f, 0.0f},
                      .spreadDegrees = 30.0f,
                      .gravity = {0.0f, 2.4f, 0.0f},
                      .drag = 0.9f,
                      .startColor = COLOR_RGB(255, 198, 96),
                      .endColor = COLOR_RGBA(176, 26, 8, 0),
                      .capacity = 2048}});
    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {.position = {3.6f, 0.9f, 2.2f}},
         .settings = {.shape = Concord::ParticleShape::Sphere,
                      .shapeExtent = {0.55f, 0.55f, 0.55f},
                      .emissionRate = 420.0f,
                      .lifetime = {0.9f, 1.5f},
                      .speed = {0.2f, 0.9f},
                      .startSize = {0.12f, 0.26f},
                      .endSize = {0.0f, 0.04f},
                      .gravity = {0.0f, 0.9f, 0.0f},
                      .drag = 1.4f,
                      .startColor = COLOR_RGB(88, 168, 255),
                      .endColor = COLOR_RGBA(24, 40, 128, 0),
                      .capacity = 2048}});
    // A plume that occludes rather than glows. Scatter blending is what makes
    // it read as smoke: an additive plume over a bright sky turns into a halo
    // instead of a shadow, which is the clearest tell that smoke is being
    // drawn as fire.
    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {.position = {-4.4f, 2.2f, -2.6f}},
         .settings = {.shape = Concord::ParticleShape::Cone,
                      .shapeExtent = {0.5f, 0.35f, 0.0f},
                      // Smaller, thinner and more numerous: one big soft blob
                      // is a cotton ball, a hundred small eroded ones are smoke.
                      .emissionRate = 48.0f,
                      .lifetime = {3.4f, 5.6f},
                      .speed = {0.55f, 1.35f},
                      .startSize = {0.6f, 1.0f},
                      // Grown to twice its birth size rather than to three
                      // times. A plume two metres from the camera at five
                      // metres across covers a third of the frame on its own,
                      // and a few hundred of those overlapping is the most
                      // expensive fill in the picture.
                      .endSize = {1.4f, 2.3f},
                      .direction = {0.10f, 1.0f, 0.06f},
                      .spreadDegrees = 15.0f,
                      .gravity = {0.05f, 0.30f, 0.0f},
                      .drag = 0.55f,
                      .startColor = COLOR_RGBA(148, 152, 158, 82),
                      .endColor = COLOR_RGBA(74, 78, 88, 0),
                      .capacity = 1024,
                      .blend = Concord::ParticleBlendMode::Scatter}});
    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = {-3.0f, 2.4f, 1.5f}})
        .Add<Concord::LightComponent>(Concord::LightComponent{
            .type = Concord::LightType::Point, .color = COLOR_RGB(64, 126, 255),
            .intensity = 6.0f, .range = 14.0f});
    // The grade is part of the scene, not a shader constant: exposure is a
    // stop, and the rest is the look. Left at a straight exposure on purpose,
    // so switching the grade on cannot quietly dim a correctly lit scene.
    scene.SetEnvironment({// Analytic sky plus a marched cloud layer. Coverage is
                          // the day cycle's, since a sunset is the light the
                          // clouds catch.
                          //
                          // The layer is deliberately far deeper than it is
                          // high. What separates a sky with clouds in it from a
                          // sky with a lid on it is whether the clouds have
                          // room to be taller than they are apart: at a depth
                          // matching the base altitude every cumulus is a
                          // pancake, and no amount of shading makes a pancake
                          // read as a body. The weather cells are scaled to
                          // match, so a cloud stays a feature inside a weather
                          // system rather than becoming the weather system.
                          .cloudDensity = 0.0080f,
                          .cloudAltitude = 420.0f,
                          .cloudThickness = 760.0f,
                          .cloudWeatherScale = 1650.0f,
                          .cloudDetailScale = 185.0f,
                          .cloudDetailStrength = 0.44f,
                          .cloudType = 0.88f,
                          .cloudSteps = 36,
                          .cloudLightSteps = 6,
                          .cloudDriftSpeed = 12.0f,
                          // Pulled back from the default now that the march
                          // sums three scattering octaves instead of one. The
                          // extra terms are the light that reached a sample by
                          // bouncing, which is most of what a real cloud is
                          // lit by, so the same gain that lit a single-scatter
                          // cloud correctly blows this one out.
                          .cloudLightGain = 3.2f,
                          .cloudAmbientGain = 1.4f,
                          .skyColor = COLOR_RGB(10, 14, 26),
                          .ambientColor = COLOR_RGB(76, 88, 120),
                          // Pulled back from 1.30 and 1.85. Ambient light is
                          // what fills shadows, and past a point it stops
                          // being fill and becomes a flat wash that lifts
                          // every dark value in the frame toward the same grey
                          // -- which is what a picture reading as pale and
                          // washed out actually is.
                          .ambientIntensity = 0.95f,
                          .exposure = 1.62f,
                          .contrast = 0.34f,
                          .saturation = 1.22f,
                          .vignette = 0.30f,
                          .bloomThreshold = 8.5f,
                          .bloomIntensity = 0.0f,
                          .bloomRadius = 14.0f,
                          .chromaticAberration = 0.0f,
                          // A medium dense enough to see, thin enough that the
                          // far side of the basin is still a place rather than a
                          // wall: the near sill stays readable, the far shore
                          // recedes, and the low sun lights the air between.
                          // Halved. The medium is the other half of the
                          // wash: at 0.055 the column in front of everything
                          // is more than half a unit of optical depth, so no
                          // surface in the frame reaches the eye at its own
                          // contrast whatever the grade does.
                          .fogDensity = 0.030f,
                          .fogHeightFalloff = 0.075f,
                          .fogBaseHeight = 1.2f,
                          .fogAnisotropy = 0.66f,
                          // Pulled back now that the medium's sun is no longer
                          // switched off in shadowed air: the term is applied
                          // everywhere instead of everywhere-but-the-shafts, so
                          // the same coefficient would read brighter than it did.
                          .fogSunScattering = 0.80f,
                          .fogAmbientScattering = 0.30f,
                          // Zero: the medium scatters the sun and the sky but
                          // does not test whether the sun reaches it.
                          //
                          // This is not a performance shortcut, it is the only
                          // setting that is correct at this budget. Each step is
                          // a shadow ray whose answer is binary, so a point in
                          // the air is either lit or not; with one step the
                          // medium darkens across the exact silhouette of every
                          // occluder's shadow volume, projected onto the ray a
                          // little way in front of the camera. What that looks
                          // like is a pale, hard-edged, offset duplicate of
                          // every object in the scene, hanging in the air --
                          // and adding steps does not remove it, it only turns
                          // one hard boundary into a staircase of them.
                          //
                          // Making it right needs many jittered samples averaged
                          // over time, which is a denoiser this renderer does
                          // not have. Until then the honest choice is haze that
                          // is lit rather than haze that fakes a shadow.
                          .fogSteps = 0,
                          .fogStepDistance = 34.0f});

    std::vector<Concord::Entity> scenery;
    scene.Query<Concord::MeshRenderer>([&](Concord::Entity entity, Concord::MeshRenderer&) {
        scenery.push_back(entity);
    });
    for (Concord::Entity entity : scenery) {
        Concord::World& world = scene.GetWorld();
        if (world.Has<Concord::Collider>(entity) || world.Has<Concord::CameraComponent>(entity) ||
            world.Has<Concord::ParticleEmitterComponent>(entity)) {
            continue;
        }
        const Concord::MeshRenderer* mesh = world.Get<Concord::MeshRenderer>(entity);
        if (mesh == nullptr || mesh->shape == Concord::PrimitiveShape::Model) {
            continue;
        }
        scene.Wrap(entity)
            .Add<Concord::Collider>(
                {.shape = mesh->shape == Concord::PrimitiveShape::Sphere
                              ? Concord::CollisionShape::Sphere
                              : Concord::CollisionShape::Box,
                 .size = mesh->size})
            .Add<Concord::RigidBody>({.motion = Concord::BodyMotion::Static});
    }
    // The basin's bed, not its surface. This slab used to sit at y=0 -- exactly
    // the water's own height -- so everything dropped into the pool stopped
    // dead on an invisible lid at the waterline, which is precisely what
    // "things land on the water as if it were solid" was: the water was never
    // solid, but there was a floor in the same plane as it. Sunk to the bed so
    // a falling body crosses the surface, splashes, slows in the volume and
    // either settles on the bottom or comes back up.
    scene.Spawn<Concord::Object::StaticBody>(
        {.transform = {.position = {0.0f, -2.1f, 0.0f}}, .size = {25.0f, 0.16f, 25.0f}});
    scene.Spawn<Concord::Object::DynamicBox>(
        {.transform = {.position = {-1.4f, 8.2f, 0.4f}, .rotation = {18.0f, 25.0f, 6.0f}},
         .size = {0.55f, 0.55f, 0.55f},
         .material = {.albedo = COLOR_RGB(204, 86, 58), .roughness = 0.35f},
         .mass = 1.4f,
         .restitution = 0.18f})
        // A wooden crate: it goes under on impact, then comes back up and
        // rides with a third of itself clear of the surface.
        .Add<Concord::Buoyancy>({.density = 0.62f});
    scene.Spawn<Concord::Object::DynamicBox>(
        {.transform = {.position = {0.2f, 9.4f, 1.1f}, .rotation = {-12.0f, 8.0f, 22.0f}},
         .size = {0.42f, 0.70f, 0.42f},
         .material = {.albedo = COLOR_RGB(62, 122, 196), .roughness = 0.30f},
         .mass = 0.9f,
         .restitution = 0.12f})
        .Add<Concord::Buoyancy>({.density = 0.48f});
    scene.Spawn<Concord::Object::DynamicSphere>(
        {.transform = {.position = {1.6f, 7.6f, -0.4f}},
         .diameter = 0.55f,
         .material = {.albedo = COLOR_RGB(236, 196, 72), .roughness = 0.28f},
         .mass = 0.8f,
         .restitution = 0.42f})
        // Denser than the crates but still lighter than water, so it sinks
        // deep on entry and surfaces slowly -- the bob the other two skip.
        .Add<Concord::Buoyancy>({.density = 0.86f, .drag = 3.2f});

    game.Systems().Add<Concord::AnimationSystem>();
    game.Systems().Add<Concord::PhysicsSystem>();
    // Audio is off for now: the waterfall loop is the loudest thing in the
    // demo and there is nothing to hear that the picture does not already say.
    // Re-register this line to bring the spatial mix back.
    if (false) {
        game.Systems().Add<Concord::AudioSystem>();
    }
    game.Systems().Add<Concord::ParticleSystem>();
    game.Systems().Add<Concord::WaterRippleSystem>();
    // Both after PhysicsSystem so this frame's linearVelocity has already been
    // copied back from Jolt: the splash reads it to size the impact, and the
    // float writes it back for the next step to integrate.
    game.Systems().Add<Concord::WaterSplashSystem>();
    game.Systems().Add<Concord::WaterBuoyancySystem>();
    // Slowed from five minutes to twenty. A five-minute day puts the sun
    // through fifteen degrees a minute, which is fast enough to watch move --
    // and a sky visibly sweeping overhead is the single loudest thing in the
    // frame, so nothing else in it can be judged while it is happening. Twenty
    // minutes still crosses a whole day in one sitting without the light
    // changing underneath whatever is being looked at.
    Concord::DayCycleSystem& dayCycle =
        game.Systems().Add<Concord::DayCycleSystem>(Concord::DayCycleSettings{
            .secondsPerDay = 1200.0f,
            .startHour = 9.5f,
            .peakSunElevationDegrees = 58.0f,
            .sunriseAzimuthDegrees = 92.0f,
            // Broken cumulus rather than overcast: enough sky left between the
            // clouds for the layer to read as bodies with gaps, which is what a
            // solid lid cannot do however well it is shaded.
            //
            // Coverage is the clock's rather than the environment's: the day
            // cycle rewrites the environment's copy every tick, so this is the
            // only place setting it has any effect.
            .cloudCoverage =
                options.cloudCoverage >= 0.0f ? options.cloudCoverage : 0.46f});
    // Drops every 140 ms: frequent enough that the surface is never still,
    // sparse enough that the rings stay readable as individual disturbances.
    // The rain field is a scene object rather than a system now, so it can be
    // aimed, tuned or removed without touching code.
    // Eight units a second is a sprint in a courtyard twenty-five across: two
    // taps of W and the whole scene has gone past. Halved, with the sprint
    // multiplier left to restore the old pace on demand, so the camera can be
    // placed rather than flown.
    Concord::FirstPersonController& controller = game.Systems().Add<Concord::FirstPersonController>(
        window, Concord::FirstPersonController::Settings{.moveSpeed = 3.6f,
                                                         .mouseSensitivity = 0.09f,
                                                         .flyMode = true,
                                                         .sprintMultiplier = 2.6f});
    game.Systems().Add<TailDemoSystem>(graph);

    game.OnUpdate([&window, overlay = &game.Overlay(), &dayCycle, &game, &controller](f32) {
        if (window.WasKeyPressed(Concord::KeyCode::F3)) {
            overlay->showDebugInfo = !overlay->showDebugInfo;
        }
        if (window.WasKeyPressed(Concord::KeyCode::N)) {
            dayCycle.SetHour(22.0f);
        }
        if (window.WasKeyPressed(Concord::KeyCode::M)) {
            dayCycle.SetHour(10.0f);
        }
        if (window.WasKeyPressed(Concord::KeyCode::V)) {
            controller.SetFlyMode(!controller.FlyMode());
        }
        Concord::UiCanvas& ui = game.Ui();
        ui.Panel(12.0f, 12.0f, 176.0f, 96.0f);
        ui.Label(20.0f, 18.0f, "Concord Flash");
        char clock[32];
        std::snprintf(clock, sizeof(clock), "hour %.1f", dayCycle.Cycle().Hour());
        ui.Label(20.0f, 38.0f, clock);
        if (ui.Button(20.0f, 60.0f, 72.0f, 24.0f, "Night")) {
            dayCycle.SetHour(22.0f);
        }
        if (ui.Button(100.0f, 60.0f, 72.0f, 24.0f, "Day")) {
            dayCycle.SetHour(10.0f);
        }
    });

    std::printf("[demo] controls: WASD move | click = mouse look, Esc = release | "
                "Space/Ctrl = fly up/down | Shift = sprint | V = walk/fly | "
                "walk: Space = jump | F3 = stats | N night / M day | "
                "close the window to quit\n");
    std::printf("[demo] shading mode: ray tracing with sun shadow rays\n");
    std::fflush(stdout);
    std::fflush(stdout);

    // Applied after the scene has authored its own values, so a flag that was
    // not passed leaves the authored one exactly as it was.
    {
        Concord::EnvironmentSettings tuned = scene.Environment();
        if (options.cloudDensity >= 0.0f) tuned.cloudDensity = options.cloudDensity;
        if (options.cloudLightGain >= 0.0f) tuned.cloudLightGain = options.cloudLightGain;
        if (options.cloudAmbientGain >= 0.0f) tuned.cloudAmbientGain = options.cloudAmbientGain;
        if (options.cloudSteps >= 0.0f) tuned.cloudSteps = static_cast<u32>(options.cloudSteps);
        if (options.cloudLightSteps >= 0.0f)
            tuned.cloudLightSteps = static_cast<u32>(options.cloudLightSteps);
        // The day cycle scales this by the hour rather than replacing it, and
        // it latches whatever it finds on its first tick, so setting it here --
        // before that tick -- is what makes the override hold.
        if (options.exposure >= 0.0f) tuned.exposure = options.exposure;
        scene.SetEnvironment(tuned);
    }

    game.LoadScene(scene);
    if (stillMode) {
        if (options.hour >= 0.0f) {
            dayCycle.SetHour(options.hour);
        }
        const bool wrote = game.RenderStill(options.stillPath.c_str(),
                                            {.warmupFrames = options.warmupFrames,
                                             .includeOverlay = options.overlay});
        std::fprintf(stderr, "[demo] still %s\n", wrote ? "written" : "FAILED");
        return wrote ? 0 : 1;
    }
    game.Run();
    return 0;
}
