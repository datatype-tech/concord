// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Concord::AssetGltf {
namespace {

bool AttributeIndex(const AssetJson::Value* attributes, std::string_view name, i32& result)
{
    if (!attributes || !attributes->Is(AssetJson::Type::Object)) return false;
    const AssetJson::Value* value = Member(*attributes, name);
    return value && SignedIndex(value, result) && result >= 0;
}

void BuildNormals(ModelPrimitive& primitive)
{
    std::vector<Vec3> sums(primitive.vertices.size());
    for (usize i = 0; i + 2 < primitive.indices.size(); i += 3) {
        const u32 ia = primitive.indices[i], ib = primitive.indices[i + 1], ic = primitive.indices[i + 2];
        const Vec3 face = Cross(primitive.vertices[ib].position - primitive.vertices[ia].position,
                                primitive.vertices[ic].position - primitive.vertices[ia].position);
        sums[ia] += face; sums[ib] += face; sums[ic] += face;
    }
    for (usize i = 0; i < primitive.vertices.size(); ++i) {
        if (Length(primitive.vertices[i].normal) < 0.0001f) primitive.vertices[i].normal = Normalize(sums[i]);
        if (Length(primitive.vertices[i].normal) < 0.0001f) primitive.vertices[i].normal = {0.0f, 1.0f, 0.0f};
    }
}

void BuildTangents(ModelPrimitive& primitive)
{
    std::vector<Vec3> sums(primitive.vertices.size());
    for (usize i = 0; i + 2 < primitive.indices.size(); i += 3) {
        ModelVertex& a = primitive.vertices[primitive.indices[i]];
        ModelVertex& b = primitive.vertices[primitive.indices[i + 1]];
        ModelVertex& c = primitive.vertices[primitive.indices[i + 2]];
        const Vec3 e1 = b.position - a.position, e2 = c.position - a.position;
        const Vec2 d1 = b.texcoord - a.texcoord, d2 = c.texcoord - a.texcoord;
        const f32 determinant = d1.x * d2.y - d2.x * d1.y;
        if (std::abs(determinant) < 0.000001f) continue;
        const Vec3 tangent = (e1 * d2.y - e2 * d1.y) / determinant;
        sums[primitive.indices[i]] += tangent; sums[primitive.indices[i + 1]] += tangent;
        sums[primitive.indices[i + 2]] += tangent;
    }
    for (usize i = 0; i < primitive.vertices.size(); ++i) {
        Vec3 tangent = sums[i];
        if (Length(tangent) < 0.0001f) tangent = Cross(primitive.vertices[i].normal, {0.0f, 1.0f, 0.0f});
        if (Length(tangent) < 0.0001f) tangent = Cross(primitive.vertices[i].normal, {1.0f, 0.0f, 0.0f});
        tangent = Normalize(tangent);
        primitive.vertices[i].tangent = {tangent.x, tangent.y, tangent.z, 1.0f};
    }
}

bool ReadPrimitive(Context& context, const AssetJson::Value& record, ModelPrimitive& primitive)
{
    const AssetJson::Value* attributes = Member(record, "attributes");
    i32 positionIndex = -1;
    if (!AttributeIndex(attributes, "POSITION", positionIndex)) return context.Fail("glTF primitive has no POSITION accessor");
    std::vector<f32> positions;
    if (!ReadFloatAccessor(context, positionIndex, 3, positions)) return context.Fail("invalid glTF POSITION accessor");
    const usize count = positions.size() / 3;
    primitive.vertices.resize(count);
    for (usize i = 0; i < count; ++i) primitive.vertices[i].position = {positions[i * 3] * context.options.scale, positions[i * 3 + 1] * context.options.scale, positions[i * 3 + 2] * context.options.scale};
    i32 index = -1;
    if (Member(record, "indices") && (!SignedIndex(Member(record, "indices"), index) || !ReadIndexAccessor(context, index, primitive.indices))) return context.Fail("invalid glTF index accessor");
    if (primitive.indices.empty()) { primitive.indices.resize(count); for (u32 i = 0; i < count; ++i) primitive.indices[i] = i; }
    u32 mode = 4;
    if (const auto* modeValue = Member(record, "mode")) {
        usize parsedMode = 0;
        if (!IndexValue(modeValue, parsedMode) || parsedMode > 6) return context.Fail("invalid glTF primitive mode");
        mode = static_cast<u32>(parsedMode);
    }
    if (mode == 5 || mode == 6) {
        std::vector<u32> triangles;
        for (usize i = 2; i < primitive.indices.size(); ++i) {
            if (mode == 5 && (i & 1u)) { triangles.insert(triangles.end(), {primitive.indices[i - 1], primitive.indices[i - 2], primitive.indices[i]}); }
            else triangles.insert(triangles.end(), {primitive.indices[i - 2], primitive.indices[i - 1], primitive.indices[i]});
        }
        primitive.indices = std::move(triangles);
    } else if (mode != 4) return context.Fail("unsupported glTF primitive mode");
    for (u32 value : primitive.indices) if (value >= count) return context.Fail("glTF index exceeds vertex count");
    std::vector<f32> values;
    if (AttributeIndex(attributes, "NORMAL", index)) {
        if (!ReadFloatAccessor(context, index, 3, values) || values.size() != count * 3) return context.Fail("invalid glTF NORMAL accessor");
        for (usize i = 0; i < count; ++i) primitive.vertices[i].normal = Normalize({values[i * 3], values[i * 3 + 1], values[i * 3 + 2]});
    } else if (context.options.generateNormals) BuildNormals(primitive);
    if (AttributeIndex(attributes, "TEXCOORD_0", index)) {
        if (!ReadFloatAccessor(context, index, 2, values) || values.size() != count * 2) return context.Fail("invalid glTF TEXCOORD_0 accessor");
        for (usize i = 0; i < count; ++i) primitive.vertices[i].texcoord = {values[i * 2], context.options.flipV ? 1.0f - values[i * 2 + 1] : values[i * 2 + 1]};
    }
    if (AttributeIndex(attributes, "TANGENT", index)) {
        if (!ReadFloatAccessor(context, index, 4, values) || values.size() != count * 4) return context.Fail("invalid glTF TANGENT accessor");
        for (usize i = 0; i < count; ++i) primitive.vertices[i].tangent = {values[i * 4], values[i * 4 + 1], values[i * 4 + 2], values[i * 4 + 3]};
    } else if (context.options.generateTangents) BuildTangents(primitive);
    if (AttributeIndex(attributes, "JOINTS_0", index)) {
        std::vector<std::array<u16, 4>> joints;
        if (!ReadJointAccessor(context, index, joints) || joints.size() != count) return context.Fail("invalid glTF JOINTS_0 accessor");
        for (usize i = 0; i < count; ++i) primitive.vertices[i].joints = joints[i];
    }
    if (AttributeIndex(attributes, "WEIGHTS_0", index)) {
        if (!ReadFloatAccessor(context, index, 4, values) || values.size() != count * 4) return context.Fail("invalid glTF WEIGHTS_0 accessor");
        for (usize i = 0; i < count; ++i) {
            const f32 x = values[i * 4], y = values[i * 4 + 1], z = values[i * 4 + 2], w = values[i * 4 + 3];
            const f32 sum = x + y + z + w;
            if (std::isfinite(sum) && sum > 0.000001f) {
                primitive.vertices[i].weights = {x / sum, y / sum, z / sum, w / sum};
            } else {
                primitive.vertices[i].weights = {};
            }
        }
    }
    return true;
}

} // namespace

bool ReadMeshes(Context& context)
{
    const AssetJson::Value* meshes = Member(*context.root, "meshes");
    if (!meshes || !meshes->Is(AssetJson::Type::Array) || meshes->array.empty()) return context.Fail("glTF meshes array is missing or empty");
    context.asset.meshes.clear(); context.asset.meshes.reserve(meshes->array.size());
    for (const AssetJson::Value& record : meshes->array) {
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF mesh");
        ModelMesh mesh{}; mesh.name = std::string(Member(record, "name") ? Member(record, "name")->String() : std::string_view{});
        const AssetJson::Value* primitives = Member(record, "primitives");
        if (!primitives || !primitives->Is(AssetJson::Type::Array) || primitives->array.empty()) return context.Fail("glTF mesh has no primitives");
        for (const AssetJson::Value& primitiveRecord : primitives->array) {
            ModelPrimitive primitive{};
            if (!ReadPrimitive(context, primitiveRecord, primitive)) return false;
            const AssetJson::Value* material = Member(primitiveRecord, "material");
            usize materialIndex = 0;
            if (material && (!IndexValue(material, materialIndex) || materialIndex > std::numeric_limits<u32>::max())) return context.Fail("invalid glTF material index");
            primitive.materialIndex = static_cast<u32>(materialIndex);
            if (primitive.materialIndex >= context.asset.materials.size()) return context.Fail("glTF material index out of range");
            mesh.primitives.push_back(std::move(primitive));
        }
        context.asset.meshes.push_back(std::move(mesh));
    }
    return true;
}

} // namespace Concord::AssetGltf
