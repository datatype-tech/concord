// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationBlendSpace2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Concord {
namespace {

constexpr f32 kDegenerateArea = 1e-12f;

/** Smallest barycentric weight, used to score how well a triangle covers a point. */
f32 Coverage(const f32 weights[3]) noexcept
{
    return std::min(weights[0], std::min(weights[1], weights[2]));
}

/** Twice the signed area of a triangle; zero when its corners are collinear. */
f32 TwiceArea(Vec2 a, Vec2 b, Vec2 c) noexcept
{
    return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
}

} // namespace

bool BlendSpace2DBarycentric(Vec2 point, Vec2 a, Vec2 b, Vec2 c, f32 weights[3]) noexcept
{
    const f32 edge0X = b.x - a.x;
    const f32 edge0Y = b.y - a.y;
    const f32 edge1X = c.x - a.x;
    const f32 edge1Y = c.y - a.y;
    const f32 offsetX = point.x - a.x;
    const f32 offsetY = point.y - a.y;
    const f32 denominator = edge0X * edge1Y - edge1X * edge0Y;
    if (!std::isfinite(denominator) || std::abs(denominator) < kDegenerateArea) {
        return false;
    }
    const f32 towardB = (offsetX * edge1Y - edge1X * offsetY) / denominator;
    const f32 towardC = (edge0X * offsetY - offsetX * edge0Y) / denominator;
    weights[0] = 1.0f - towardB - towardC;
    weights[1] = towardB;
    weights[2] = towardC;
    return std::isfinite(weights[0]) && std::isfinite(weights[1]) && std::isfinite(weights[2]);
}

bool BlendSpace2D::IsValid() const noexcept
{
    if (samples.empty() || triangles.empty()) {
        return false;
    }
    for (const BlendSpace2Sample& sample : samples) {
        if (sample.clip == nullptr || !std::isfinite(sample.position.x) ||
            !std::isfinite(sample.position.y) || !std::isfinite(sample.speed) ||
            sample.speed <= 0.0f) {
            return false;
        }
    }
    for (const BlendSpace2Triangle& triangle : triangles) {
        if (triangle[0] >= samples.size() || triangle[1] >= samples.size() ||
            triangle[2] >= samples.size() || triangle[0] == triangle[1] ||
            triangle[1] == triangle[2] || triangle[0] == triangle[2]) {
            return false;
        }
        // Collinear corners enclose no area, so Evaluate can never resolve a
        // point inside them. Accepting such a triangle here would let IsValid
        // promise something the evaluator cannot deliver, using the same
        // threshold that makes the barycentric solve refuse it.
        const f32 area = TwiceArea(samples[triangle[0]].position, samples[triangle[1]].position,
                                   samples[triangle[2]].position);
        if (!std::isfinite(area) || std::abs(area) < kDegenerateArea) {
            return false;
        }
    }
    return true;
}

bool BlendSpace2D::Evaluate(Vec2 value, std::array<BlendSpace2Weight, 3>& out,
                            u32& count) const noexcept
{
    out = {};
    count = 0;
    if (!IsValid() || !std::isfinite(value.x) || !std::isfinite(value.y)) {
        return false;
    }
    const BlendSpace2Triangle* best = nullptr;
    f32 bestWeights[3] = {0.0f, 0.0f, 0.0f};
    // A finite sentinel is not safe here: a point can sit far enough outside
    // a triangle for its coverage to equal any constant, and then no triangle
    // would ever be selected.
    f32 bestCoverage = -std::numeric_limits<f32>::infinity();
    for (const BlendSpace2Triangle& triangle : triangles) {
        f32 weights[3] = {0.0f, 0.0f, 0.0f};
        if (!BlendSpace2DBarycentric(value, samples[triangle[0]].position,
                                     samples[triangle[1]].position,
                                     samples[triangle[2]].position, weights)) {
            continue;
        }
        const f32 coverage = Coverage(weights);
        if (coverage > bestCoverage) {
            bestCoverage = coverage;
            best = &triangle;
            bestWeights[0] = weights[0];
            bestWeights[1] = weights[1];
            bestWeights[2] = weights[2];
        }
    }
    if (best == nullptr) {
        return false;
    }
    // Drop the weights that pull the point outside this triangle and share the
    // remainder, so an off-hull value blends the clipped edge rather than
    // extrapolating a pose.
    f32 total = 0.0f;
    for (u32 corner = 0; corner < 3; ++corner) {
        bestWeights[corner] = std::max(bestWeights[corner], 0.0f);
        total += bestWeights[corner];
    }
    if (!(total > 0.0f)) {
        return false;
    }
    for (u32 corner = 0; corner < 3; ++corner) {
        if (bestWeights[corner] <= 0.0f) {
            continue;
        }
        out[count++] = BlendSpace2Weight{.sample = (*best)[corner],
                                         .weight = bestWeights[corner] / total};
    }
    return count != 0;
}

} // namespace Concord
