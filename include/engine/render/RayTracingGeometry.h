// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RAYTRACINGGEOMETRY_H
#define CONCORD_RAYTRACINGGEOMETRY_H

#include "engine/asset/ModelAsset.h"
#include "engine/core/Types.h"

#include <limits>
#include <vector>

namespace Concord {

inline constexpr u32 kRayTracingPositionStrideBytes = sizeof(Vec3);
inline constexpr u32 kRayTracingIndexStrideBytes = sizeof(u32);

/** Describes why a CPU primitive cannot be used to build a triangle BLAS. */
enum class RayTracingGeometryError {
    None,
    EmptyVertices,
    UnsupportedMode,
    InvalidIndexCount,
    IndexOutOfRange,
    NonFinitePosition,
    DegenerateTriangle,
    CountOverflow,
    AllocationFailure,
};

/** Owned, validated position/index data ready for a future BLAS upload. */
struct RayTracingGeometryDescriptor {
    std::vector<Vec3> positions;
    std::vector<u32> indices;
    u32 materialIndex = 0;

    /** Number of vertices represented by the descriptor, or zero on overflow. */
    [[nodiscard]] u32 VertexCount() const noexcept
    {
        return positions.size() <= std::numeric_limits<u32>::max()
                   ? static_cast<u32>(positions.size())
                   : 0;
    }

    /** Number of indices represented by the descriptor, or zero on overflow. */
    [[nodiscard]] u32 IndexCount() const noexcept
    {
        return indices.size() <= std::numeric_limits<u32>::max()
                   ? static_cast<u32>(indices.size())
                   : 0;
    }

    /** Number of indexed triangles represented by the descriptor. */
    [[nodiscard]] u32 PrimitiveCount() const noexcept
    {
        return IndexCount() >= 3 && IndexCount() % 3 == 0 ? IndexCount() / 3 : 0;
    }

    /** Whether the descriptor has a structurally valid indexed triangle list. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return VertexCount() != 0 && PrimitiveCount() != 0;
    }
};

/** Converts and validates one imported triangle primitive for RT consumption. */
bool BuildRayTracingGeometryDescriptor(const ModelPrimitive& primitive,
                                       RayTracingGeometryDescriptor& descriptor,
                                       RayTracingGeometryError* error = nullptr);

/** Returns a stable diagnostic string for a geometry validation result. */
[[nodiscard]] const char* RayTracingGeometryErrorMessage(
    RayTracingGeometryError error) noexcept;

} // namespace Concord

#endif // CONCORD_RAYTRACINGGEOMETRY_H
