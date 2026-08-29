// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RayTracingGeometry.h"

#include <cmath>
#include <utility>

namespace Concord {
namespace {

constexpr f32 kMinimumTriangleAreaSquared = 1.0e-20f;

void SetError(RayTracingGeometryError* output, RayTracingGeometryError error) noexcept
{
    if (output != nullptr) {
        *output = error;
    }
}

bool Finite(Vec3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool ValidateTriangle(const std::vector<ModelVertex>& vertices, const std::vector<u32>& indices,
                      usize offset, RayTracingGeometryError* error) noexcept
{
    const u32 ia = indices[offset];
    const u32 ib = indices[offset + 1];
    const u32 ic = indices[offset + 2];
    if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size()) {
        SetError(error, RayTracingGeometryError::IndexOutOfRange);
        return false;
    }
    const Vec3 edgeA = vertices[ib].position - vertices[ia].position;
    const Vec3 edgeB = vertices[ic].position - vertices[ia].position;
    const Vec3 cross = Cross(edgeA, edgeB);
    if (!Finite(cross) || Dot(cross, cross) <= kMinimumTriangleAreaSquared) {
        SetError(error, RayTracingGeometryError::DegenerateTriangle);
        return false;
    }
    return true;
}

} // namespace

const char* RayTracingGeometryErrorMessage(RayTracingGeometryError error) noexcept
{
    switch (error) {
    case RayTracingGeometryError::None: return "none";
    case RayTracingGeometryError::EmptyVertices: return "primitive has no vertices";
    case RayTracingGeometryError::UnsupportedMode: return "primitive mode is not triangles";
    case RayTracingGeometryError::InvalidIndexCount: return "index count is not a triangle list";
    case RayTracingGeometryError::IndexOutOfRange: return "triangle index exceeds vertex count";
    case RayTracingGeometryError::NonFinitePosition: return "vertex position is not finite";
    case RayTracingGeometryError::DegenerateTriangle: return "triangle has zero area";
    case RayTracingGeometryError::CountOverflow: return "geometry count exceeds 32-bit range";
    case RayTracingGeometryError::AllocationFailure: return "geometry allocation failed";
    }
    return "unknown geometry error";
}

bool BuildRayTracingGeometryDescriptor(const ModelPrimitive& primitive,
                                       RayTracingGeometryDescriptor& descriptor,
                                       RayTracingGeometryError* error)
{
    descriptor = {};
    SetError(error, RayTracingGeometryError::None);
    if (primitive.vertices.empty()) {
        SetError(error, RayTracingGeometryError::EmptyVertices);
        return false;
    }
    if (primitive.mode != 4) {
        SetError(error, RayTracingGeometryError::UnsupportedMode);
        return false;
    }
    if (primitive.vertices.size() > std::numeric_limits<u32>::max() ||
        primitive.indices.size() > std::numeric_limits<u32>::max()) {
        SetError(error, RayTracingGeometryError::CountOverflow);
        return false;
    }
    for (const ModelVertex& vertex : primitive.vertices) {
        if (!Finite(vertex.position)) {
            SetError(error, RayTracingGeometryError::NonFinitePosition);
            return false;
        }
    }
    std::vector<u32> indices;
    try {
        if (primitive.indices.empty()) {
            if (primitive.vertices.size() < 3 || primitive.vertices.size() % 3 != 0) {
                SetError(error, RayTracingGeometryError::InvalidIndexCount);
                return false;
            }
            indices.resize(primitive.vertices.size());
            for (u32 index = 0; index < indices.size(); ++index) {
                indices[index] = index;
            }
        } else {
            if (primitive.indices.size() < 3 || primitive.indices.size() % 3 != 0) {
                SetError(error, RayTracingGeometryError::InvalidIndexCount);
                return false;
            }
            indices = primitive.indices;
        }
        for (usize offset = 0; offset < indices.size(); offset += 3) {
            if (!ValidateTriangle(primitive.vertices, indices, offset, error)) {
                return false;
            }
        }
        descriptor.positions.reserve(primitive.vertices.size());
        for (const ModelVertex& vertex : primitive.vertices) {
            descriptor.positions.push_back(vertex.position);
        }
        descriptor.indices = std::move(indices);
        descriptor.materialIndex = primitive.materialIndex;
    } catch (...) {
        descriptor = {};
        SetError(error, RayTracingGeometryError::AllocationFailure);
        return false;
    }
    return descriptor.IsReady();
}

} // namespace Concord
