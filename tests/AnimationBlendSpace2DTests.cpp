// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using Concord::AnimationClip;
using Concord::BlendSpace2D;
using Concord::BlendSpace2Sample;
using Concord::BlendSpace2Weight;
using Concord::Vec2;

constexpr Concord::f32 kTolerance = 0.0005f;

bool Near(Concord::f32 a, Concord::f32 b) { return std::abs(a - b) < kTolerance; }

AnimationClip g_idle;
AnimationClip g_walk;
AnimationClip g_run;

/** A right triangle: origin, +X, +Y. */
BlendSpace2D MakeTriangleSpace()
{
    BlendSpace2D space;
    space.samples.push_back(BlendSpace2Sample{.position = {0.0f, 0.0f}, .clip = &g_idle});
    space.samples.push_back(BlendSpace2Sample{.position = {2.0f, 0.0f}, .clip = &g_walk});
    space.samples.push_back(BlendSpace2Sample{.position = {0.0f, 2.0f}, .clip = &g_run});
    space.triangles.push_back({0, 1, 2});
    return space;
}

bool TestBarycentricDegenerateTriangleIsRefused()
{
    Concord::f32 weights[3] = {9.0f, 9.0f, 9.0f};
    // Collinear corners have no area and no interior to interpolate.
    if (Concord::BlendSpace2DBarycentric({1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f},
                                         {2.0f, 2.0f}, weights)) {
        return false;
    }
    return Concord::f32(weights[0]) == 9.0f;
}

bool TestBarycentricAtCornersAndCentroid()
{
    const Vec2 a{0.0f, 0.0f};
    const Vec2 b{2.0f, 0.0f};
    const Vec2 c{0.0f, 2.0f};
    Concord::f32 weights[3];
    if (!Concord::BlendSpace2DBarycentric(a, a, b, c, weights)) return false;
    if (!Near(weights[0], 1.0f) || !Near(weights[1], 0.0f)) return false;
    if (!Concord::BlendSpace2DBarycentric(c, a, b, c, weights)) return false;
    if (!Near(weights[2], 1.0f)) return false;
    if (!Concord::BlendSpace2DBarycentric({0.6666667f, 0.6666667f}, a, b, c, weights)) return false;
    return Near(weights[0], 1.0f / 3.0f) && Near(weights[1], 1.0f / 3.0f) &&
           Near(weights[2], 1.0f / 3.0f);
}

bool TestSpaceValidity()
{
    if (!MakeTriangleSpace().IsValid()) return false;
    BlendSpace2D empty;
    if (empty.IsValid()) return false;
    // A triangle referencing a missing sample cannot be evaluated.
    BlendSpace2D stray = MakeTriangleSpace();
    stray.triangles[0] = {0, 1, 9};
    if (stray.IsValid()) return false;
    // A degenerate index triple has no area either.
    BlendSpace2D repeated = MakeTriangleSpace();
    repeated.triangles[0] = {0, 1, 1};
    if (repeated.IsValid()) return false;
    BlendSpace2D nullClip = MakeTriangleSpace();
    nullClip.samples[1].clip = nullptr;
    if (nullClip.IsValid()) return false;
    BlendSpace2D badSpeed = MakeTriangleSpace();
    badSpeed.samples[2].speed = 0.0f;
    return !badSpeed.IsValid();
}

bool TestPointInsideBlendsAllThree()
{
    const BlendSpace2D space = MakeTriangleSpace();
    std::array<BlendSpace2Weight, 3> out{};
    Concord::u32 count = 0;
    if (!space.Evaluate({0.5f, 0.5f}, out, count)) return false;
    if (count != 3) return false;
    Concord::f32 total = 0.0f;
    for (Concord::u32 index = 0; index < count; ++index) total += out[index].weight;
    return Near(total, 1.0f);
}

bool TestPointOnAVertexYieldsOneContribution()
{
    const BlendSpace2D space = MakeTriangleSpace();
    std::array<BlendSpace2Weight, 3> out{};
    Concord::u32 count = 0;
    if (!space.Evaluate({0.0f, 2.0f}, out, count)) return false;
    return count == 1 && out[0].sample == 2 && Near(out[0].weight, 1.0f);
}

bool TestPointOutsideTheHullClampsToTheNearestEdge()
{
    const BlendSpace2D space = MakeTriangleSpace();
    std::array<BlendSpace2Weight, 3> out{};
    Concord::u32 count = 0;
    // (3, 3) lies beyond the hypotenuse, so the triangle's third corner pulls
    // it outside and must be dropped rather than extrapolated.
    if (!space.Evaluate({3.0f, 3.0f}, out, count)) return false;
    if (count != 2) return false;
    Concord::f32 total = 0.0f;
    for (Concord::u32 index = 0; index < count; ++index) total += out[index].weight;
    return Near(total, 1.0f) && Near(out[0].weight, 0.5f) && Near(out[1].weight, 0.5f);
}

bool TestMalformedQueriesAreRefused()
{
    const BlendSpace2D space = MakeTriangleSpace();
    std::array<BlendSpace2Weight, 3> out{};
    Concord::u32 count = 0;
    const Concord::f32 nan = std::numeric_limits<Concord::f32>::quiet_NaN();
    if (space.Evaluate({nan, 0.0f}, out, count)) return false;
    if (space.Evaluate({0.0f, nan}, out, count)) return false;
    const BlendSpace2D invalid;
    return !invalid.Evaluate({0.0f, 0.0f}, out, count) && count == 0;
}

bool TestCollinearTrianglesAreRejected()
{
    // Distinct, in-range indices and valid clips, but the corners are
    // collinear: no point can ever resolve inside, so the space must not
    // report itself usable.
    BlendSpace2D space;
    space.samples.push_back(BlendSpace2Sample{.position = {0.0f, 0.0f}, .clip = &g_idle});
    space.samples.push_back(BlendSpace2Sample{.position = {1.0f, 1.0f}, .clip = &g_walk});
    space.samples.push_back(BlendSpace2Sample{.position = {2.0f, 2.0f}, .clip = &g_run});
    space.triangles.push_back({0, 1, 2});
    if (space.IsValid()) return false;
    std::array<BlendSpace2Weight, 3> out{};
    Concord::u32 count = 0;
    return !space.Evaluate({1.0f, 1.0f}, out, count) && count == 0;
}

bool TestResetArmsThePlayhead()
{
    const BlendSpace2D space = MakeTriangleSpace();
    Concord::BlendSpace2State state;
    state.phase = 0.7f;
    if (!Concord::ResetBlendSpace2D(space, state) || !Near(state.phase, 0.0f)) return false;
    const BlendSpace2D invalid;
    state.phase = 0.4f;
    return !Concord::ResetBlendSpace2D(invalid, state) && Near(state.phase, 0.0f);
}

bool TestSamplingRefusesInvalidInput()
{
    Concord::BlendSpace2State state;
    Concord::Skeleton skeleton;
    Concord::SkeletonPose output;
    std::array<Concord::SkeletonPose, 2> scratch{};
    const BlendSpace2D invalid;
    if (Concord::SampleBlendSpace2D(invalid, state, {0.0f, 0.0f}, 0.016f, skeleton, output,
                                    scratch)) {
        return false;
    }
    const Concord::f32 nan = std::numeric_limits<Concord::f32>::quiet_NaN();
    return !Concord::SampleBlendSpace2D(MakeTriangleSpace(), state, {nan, nan}, 0.016f, skeleton,
                                        output, scratch);
}

bool TestSamplingRequiresScratchForEveryExtraContribution()
{
    const BlendSpace2D space = MakeTriangleSpace();
    Concord::BlendSpace2State state;
    Concord::Skeleton skeleton;
    Concord::SkeletonPose output;
    std::array<Concord::SkeletonPose, 2> full{};
    // A point inside the triangle resolves three clips, which needs two scratch
    // poses; offering none must be refused rather than silently dropping clips.
    if (Concord::SampleBlendSpace2D(space, state, {0.5f, 0.5f}, 0.016f, skeleton, output,
                                    std::span<Concord::SkeletonPose>{})) {
        return false;
    }
    // One contribution needs no scratch at all on that same path.
    return Concord::SampleBlendSpace2D(space, state, {0.0f, 2.0f}, 0.016f, skeleton, output,
                                       std::span<Concord::SkeletonPose>{});
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"degenerate triangle is refused", TestBarycentricDegenerateTriangleIsRefused},
        {"barycentric at corners and centroid", TestBarycentricAtCornersAndCentroid},
        {"space validity", TestSpaceValidity},
        {"point inside blends all three", TestPointInsideBlendsAllThree},
        {"point on a vertex yields one contribution", TestPointOnAVertexYieldsOneContribution},
        {"point outside the hull clamps to the nearest edge",
         TestPointOutsideTheHullClampsToTheNearestEdge},
        {"malformed queries are refused", TestMalformedQueriesAreRefused},
        {"collinear triangles are rejected", TestCollinearTrianglesAreRejected},
        {"reset arms the playhead", TestResetArmsThePlayhead},
        {"sampling refuses invalid input", TestSamplingRefusesInvalidInput},
        {"sampling requires scratch per extra contribution",
         TestSamplingRequiresScratchForEveryExtraContribution},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const Concord::u32 total = static_cast<Concord::u32>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " 2D blend space cases passed\n";
    return failures == 0 ? 0 : 1;
}
