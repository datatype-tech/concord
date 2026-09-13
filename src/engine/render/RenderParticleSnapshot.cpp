// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderParticleSnapshot.h"

#include "engine/core/Mat4.h"
#include "engine/core/Transform.h"
#include "engine/ecs/ParticleComponents.h"
#include "engine/ecs/World.h"
#include "engine/particle/ParticleSimulation.h"
#include "engine/render/RenderParticleLimits.h"

#include <cmath>

namespace Concord {
namespace {

/** Quad corners in the order the two triangles consume them. */
constexpr u32 kCornerOrder[kParticleIndicesPerParticle] = {0, 1, 2, 0, 2, 3};

constexpr Vec2 kCornerTexcoord[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

/** Reads the camera right and up axes out of the rows of its view matrix. */
void CameraAxes(const Mat4& view, Vec3& right, Vec3& up) noexcept
{
    right = {view.col[0].x, view.col[1].x, view.col[2].x};
    up = {view.col[0].y, view.col[1].y, view.col[2].y};
    right = Length(right) > 0.0001f ? Normalize(right) : Vec3{1.0f, 0.0f, 0.0f};
    up = Length(up) > 0.0001f ? Normalize(up) : Vec3{0.0f, 1.0f, 0.0f};
}

/** Transforms an affine point, ignoring the homogeneous divide. */
Vec3 TransformPoint(const Mat4& matrix, Vec3 point) noexcept
{
    const Vec4 result = matrix * Vec4{point.x, point.y, point.z, 1.0f};
    return {result.x, result.y, result.z};
}

/** Appends the two triangles of one camera-facing quad. */
void AppendBillboard(RenderParticleSnapshot& out, Vec3 center, Vec3 right, Vec3 up,
                     f32 halfSize, Vec4 color)
{
    const Vec3 rx = right * halfSize;
    const Vec3 uy = up * halfSize;
    const Vec3 corners[4] = {center - rx - uy, center + rx - uy, center + rx + uy,
                             center - rx + uy};
    for (const u32 corner : kCornerOrder) {
        out.vertices.push_back(
            RenderParticleVertex{corners[corner], kCornerTexcoord[corner], color});
    }
}

/** Expands one emitter pool into billboards, stopping at the vertex cap. */
void AppendEmitter(RenderParticleSnapshot& out, const ParticleEmitterComponent& emitter,
                   const Mat4& model, Vec3 right, Vec3 up) noexcept
{
    const ParticleEmitterSettings& settings = emitter.settings;
    if (!emitter.state.IsReady() || emitter.state.liveCount == 0) {
        return;
    }
    for (u32 index = 0; index < emitter.state.liveCount; ++index) {
        if (out.vertices.size() + kParticleIndicesPerParticle > kMaxParticleVertices) {
            return;
        }
        const Particle& particle = emitter.state.particles[index];
        const f32 fraction = ParticleLifeFraction(particle);
        const f32 size = ParticleSizeAt(particle, fraction);
        const Vec4 color = ParticleColorAt(settings, fraction);
        if (!(size > 0.0001f) || !(color.w > 0.0005f)) {
            continue;
        }
        const Vec3 center =
            settings.localSpace ? TransformPoint(model, particle.position) : particle.position;
        AppendBillboard(out, center, right, up, size * 0.5f, color);
        ++out.particleCount;
    }
}

} // namespace

void AppendParticleSnapshots(RenderParticleSnapshot& particles, const World& world,
                             const Mat4& view)
{
    if (particles.vertices.size() >= kMaxParticleVertices) {
        return;
    }
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    CameraAxes(view, right, up);

    // Two runs, additive first, so the frame is still one upload and one vertex
    // buffer while the two kinds of emitter can still be drawn with the two
    // blend states they need. Splitting the array is cheaper than splitting the
    // buffer, and the counts are all the draw needs to know.
    RenderParticleSnapshot additive{};
    RenderParticleSnapshot scattering{};
    world.Query<ParticleEmitterComponent, Transform>(
        [&additive, &scattering, right, up](Entity, const ParticleEmitterComponent& emitter,
                                            const Transform& transform) {
            RenderParticleSnapshot& target =
                emitter.settings.blend == ParticleBlendMode::Additive ? additive : scattering;
            AppendEmitter(target, emitter, transform.ToMatrix(), right, up);
        });
    particles.vertices.reserve(additive.vertices.size() + scattering.vertices.size());
    particles.vertices.insert(particles.vertices.end(), additive.vertices.begin(),
                              additive.vertices.end());
    particles.additiveVertices = static_cast<u32>(additive.vertices.size());
    particles.vertices.insert(particles.vertices.end(), scattering.vertices.begin(),
                              scattering.vertices.end());
    particles.particleCount = additive.particleCount + scattering.particleCount;
}

} // namespace Concord
