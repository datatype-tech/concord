// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/particle/ParticleSampling.h"

#include "engine/core/Angle.h"

#include <cmath>

namespace Concord {
namespace {

constexpr f32 kTwoPi = 6.28318530718f;
constexpr u32 kRandomSeedFallback = 0x9E3779B9u;

/** Picks an axis that is never parallel to the input. */
Vec3 HelperAxis(Vec3 axis) noexcept
{
    return std::abs(axis.x) < 0.9f ? Vec3{1.0f, 0.0f, 0.0f} : Vec3{0.0f, 1.0f, 0.0f};
}

/** Samples a direction uniformly on the unit sphere. */
Vec3 SampleUnitSphere(u32& state) noexcept
{
    const f32 z = SampleParticleUnit(state) * 2.0f - 1.0f;
    const f32 phi = SampleParticleUnit(state) * kTwoPi;
    const f32 radius = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return {radius * std::cos(phi), radius * std::sin(phi), z};
}

/** Samples a box or sphere offset from the emitter origin. */
Vec3 SampleVolumeOffset(const ParticleEmitterSettings& settings, u32& state) noexcept
{
    const Vec3 extent = settings.shapeExtent;
    switch (settings.shape) {
        case ParticleShape::Sphere: {
            const f32 radius = std::max(extent.x, 0.0f);
            // The cube root keeps the distribution uniform by volume instead
            // of clustering samples at the centre.
            const f32 scaled = radius * std::cbrt(SampleParticleUnit(state));
            return SampleUnitSphere(state) * scaled;
        }
        case ParticleShape::Box: {
            return {extent.x * (SampleParticleUnit(state) * 2.0f - 1.0f),
                    extent.y * (SampleParticleUnit(state) * 2.0f - 1.0f),
                    extent.z * (SampleParticleUnit(state) * 2.0f - 1.0f)};
        }
        case ParticleShape::Cone: {
            const f32 radius = std::max(extent.x, 0.0f) * std::sqrt(SampleParticleUnit(state));
            const f32 phi = SampleParticleUnit(state) * kTwoPi;
            const f32 height = std::max(extent.y, 0.0f) * SampleParticleUnit(state);
            return {radius * std::cos(phi), height, radius * std::sin(phi)};
        }
        case ParticleShape::Point:
        default:
            return {};
    }
}

} // namespace

u32 NextParticleRandom(u32& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    if (state == 0u) {
        state = kRandomSeedFallback;
    }
    return state;
}

f32 SampleParticleUnit(u32& state) noexcept
{
    // The low bits of an xorshift word are weaker than the high ones.
    return static_cast<f32>(NextParticleRandom(state) >> 8) * (1.0f / 16777216.0f);
}

void BuildParticleBasis(Vec3 axis, Vec3& tangent, Vec3& bitangent) noexcept
{
    const Vec3 helper = HelperAxis(axis);
    tangent = Normalize(Cross(helper, axis));
    bitangent = Cross(axis, tangent);
}

Vec3 SampleParticleDirection(const ParticleEmitterSettings& settings, u32& state) noexcept
{
    Vec3 axis = Normalize(settings.direction);
    if (Length(axis) < 0.0001f) {
        axis = {0.0f, 1.0f, 0.0f};
    }
    const f32 halfAngle = Radians(std::min(std::max(settings.spreadDegrees, 0.0f), 180.0f));
    const f32 cosHalf = std::cos(halfAngle);
    const f32 cosAngle = 1.0f - SampleParticleUnit(state) * (1.0f - cosHalf);
    const f32 sinAngle = std::sqrt(std::max(0.0f, 1.0f - cosAngle * cosAngle));
    const f32 phi = SampleParticleUnit(state) * kTwoPi;
    Vec3 tangent{1.0f, 0.0f, 0.0f};
    Vec3 bitangent{0.0f, 0.0f, 1.0f};
    BuildParticleBasis(axis, tangent, bitangent);
    return Normalize(tangent * (sinAngle * std::cos(phi)) +
                     bitangent * (sinAngle * std::sin(phi)) + axis * cosAngle);
}

Vec3 SampleParticleSpawn(const ParticleEmitterSettings& settings, u32& state) noexcept
{
    return SampleVolumeOffset(settings, state);
}

Vec3 TransformParticleDirection(const Mat4& matrix, Vec3 direction) noexcept
{
    return {matrix.col[0].x * direction.x + matrix.col[1].x * direction.y +
                matrix.col[2].x * direction.z,
            matrix.col[0].y * direction.x + matrix.col[1].y * direction.y +
                matrix.col[2].y * direction.z,
            matrix.col[0].z * direction.x + matrix.col[1].z * direction.y +
                matrix.col[2].z * direction.z};
}

} // namespace Concord
