// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RayTracingGeometry.h"

#include <cmath>
#include <limits>
#include <vector>

namespace {

Concord::ModelPrimitive Triangle(bool indexed = true)
{
    Concord::ModelPrimitive primitive{};
    primitive.vertices = {
        Concord::ModelVertex{.position = {0.0f, 0.0f, 0.0f}},
        Concord::ModelVertex{.position = {1.0f, 0.0f, 0.0f}},
        Concord::ModelVertex{.position = {0.0f, 1.0f, 0.0f}},
    };
    if (indexed) {
        primitive.indices = {0, 1, 2};
    }
    primitive.materialIndex = 7;
    return primitive;
}

bool TestValidIndexed()
{
    Concord::RayTracingGeometryDescriptor descriptor{};
    Concord::RayTracingGeometryError error = Concord::RayTracingGeometryError::None;
    if (!Concord::BuildRayTracingGeometryDescriptor(Triangle(), descriptor, &error) ||
        error != Concord::RayTracingGeometryError::None || !descriptor.IsReady() ||
        descriptor.VertexCount() != 3 || descriptor.IndexCount() != 3 ||
        descriptor.PrimitiveCount() != 1 || descriptor.materialIndex != 7 ||
        descriptor.indices != std::vector<Concord::u32>{0, 1, 2}) {
        return false;
    }
    return descriptor.positions[1].x == 1.0f;
}

bool TestValidNonIndexed()
{
    Concord::RayTracingGeometryDescriptor descriptor{};
    return Concord::BuildRayTracingGeometryDescriptor(Triangle(false), descriptor) &&
           descriptor.indices == std::vector<Concord::u32>{0, 1, 2};
}

bool TestRejects(Concord::ModelPrimitive primitive,
                 Concord::RayTracingGeometryError expected)
{
    Concord::RayTracingGeometryDescriptor descriptor{};
    Concord::RayTracingGeometryError error = Concord::RayTracingGeometryError::None;
    return !Concord::BuildRayTracingGeometryDescriptor(primitive, descriptor, &error) &&
           error == expected && !descriptor.IsReady();
}

bool TestInvalidInputs()
{
    Concord::ModelPrimitive empty{};
    if (!TestRejects(empty, Concord::RayTracingGeometryError::EmptyVertices)) {
        return false;
    }
    Concord::ModelPrimitive mode = Triangle();
    mode.mode = 5;
    if (!TestRejects(mode, Concord::RayTracingGeometryError::UnsupportedMode)) {
        return false;
    }
    Concord::ModelPrimitive count = Triangle();
    count.indices = {0, 1};
    if (!TestRejects(count, Concord::RayTracingGeometryError::InvalidIndexCount)) {
        return false;
    }
    Concord::ModelPrimitive range = Triangle();
    range.indices[2] = 9;
    if (!TestRejects(range, Concord::RayTracingGeometryError::IndexOutOfRange)) {
        return false;
    }
    Concord::ModelPrimitive nonFinite = Triangle();
    nonFinite.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    if (!TestRejects(nonFinite, Concord::RayTracingGeometryError::NonFinitePosition)) {
        return false;
    }
    Concord::ModelPrimitive degenerate = Triangle();
    degenerate.vertices[2].position = {2.0f, 0.0f, 0.0f};
    return TestRejects(degenerate, Concord::RayTracingGeometryError::DegenerateTriangle);
}

} // namespace

int main()
{
    return TestValidIndexed() && TestValidNonIndexed() && TestInvalidInputs() ? 0 : 1;
}
