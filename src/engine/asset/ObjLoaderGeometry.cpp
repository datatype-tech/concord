// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ObjLoaderInternal.h"

#include <cmath>

namespace Concord::AssetObj {

void GenerateNormals(Builder& builder)
{
    for (ModelMesh& mesh : builder.asset.meshes) {
        for (ModelPrimitive& primitive : mesh.primitives) {
            std::vector<Vec3> sums(primitive.vertices.size());
            std::vector<bool> missing(primitive.vertices.size());
            for (usize i = 0; i < primitive.vertices.size(); ++i) {
                const Vec3 normal = primitive.vertices[i].normal;
                missing[i] = !std::isfinite(normal.x) || !std::isfinite(normal.y) ||
                             !std::isfinite(normal.z) || Length(normal) < 0.0001f;
            }
            for (usize i = 0; i + 2 < primitive.indices.size(); i += 3) {
                ModelVertex& a = primitive.vertices[primitive.indices[i]];
                ModelVertex& b = primitive.vertices[primitive.indices[i + 1]];
                ModelVertex& c = primitive.vertices[primitive.indices[i + 2]];
                const Vec3 face = Cross(b.position - a.position, c.position - a.position);
                for (u32 index : {primitive.indices[i], primitive.indices[i + 1], primitive.indices[i + 2]}) {
                    if (missing[index]) sums[index] += face;
                }
            }
            for (usize i = 0; i < primitive.vertices.size(); ++i) {
                if (missing[i] && std::isfinite(sums[i].x) && std::isfinite(sums[i].y) && std::isfinite(sums[i].z)) primitive.vertices[i].normal = Normalize(sums[i]);
                if (Length(primitive.vertices[i].normal) < 0.0001f) primitive.vertices[i].normal = {0.0f, 1.0f, 0.0f};
            }
        }
    }
}

void GenerateTangents(Builder& builder)
{
    for (ModelMesh& mesh : builder.asset.meshes) {
        for (ModelPrimitive& primitive : mesh.primitives) {
            std::vector<Vec3> sums(primitive.vertices.size());
            for (usize i = 0; i + 2 < primitive.indices.size(); i += 3) {
                ModelVertex& a = primitive.vertices[primitive.indices[i]];
                ModelVertex& b = primitive.vertices[primitive.indices[i + 1]];
                ModelVertex& c = primitive.vertices[primitive.indices[i + 2]];
                const Vec3 edge1 = b.position - a.position, edge2 = c.position - a.position;
                const Vec2 uv1 = b.texcoord - a.texcoord, uv2 = c.texcoord - a.texcoord;
                const f32 determinant = uv1.x * uv2.y - uv2.x * uv1.y;
                if (std::abs(determinant) < 0.000001f) continue;
                const Vec3 tangent = (edge1 * uv2.y - edge2 * uv1.y) / determinant;
                sums[primitive.indices[i]] += tangent;
                sums[primitive.indices[i + 1]] += tangent;
                sums[primitive.indices[i + 2]] += tangent;
            }
            for (usize i = 0; i < primitive.vertices.size(); ++i) {
                Vec3 tangent = sums[i];
                if (!std::isfinite(tangent.x) || !std::isfinite(tangent.y) || !std::isfinite(tangent.z)) tangent = {};
                if (Length(tangent) < 0.0001f) {
                    tangent = Cross(primitive.vertices[i].normal, {0.0f, 1.0f, 0.0f});
                    if (Length(tangent) < 0.0001f) tangent = Cross(primitive.vertices[i].normal, {1.0f, 0.0f, 0.0f});
                }
                primitive.vertices[i].tangent = {Normalize(tangent).x, Normalize(tangent).y,
                                                  Normalize(tangent).z, 1.0f};
            }
        }
    }
}

} // namespace Concord::AssetObj
