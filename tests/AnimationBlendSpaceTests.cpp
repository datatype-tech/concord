// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using Concord::AnimationClip;
using Concord::BlendSpace1D;
using Concord::BlendSpaceSample;
using Concord::BlendSpaceState;
using Concord::f32;
using Concord::usize;

constexpr f32 kTolerance = 0.0005f;

bool Near(f32 a, f32 b) { return std::abs(a - b) < kTolerance; }

/** Walk at 0, jog at 2, run at 6: a typical locomotion axis. */
AnimationClip g_walk;
AnimationClip g_jog;
AnimationClip g_run;

BlendSpace1D MakeLocomotionSpace()
{
    g_walk.duration = 1.0f;
    g_jog.duration = 0.75f;
    g_run.duration = 0.5f;
    BlendSpace1D space;
    space.samples.push_back(BlendSpaceSample{.position = 0.0f, .clip = &g_walk});
    space.samples.push_back(BlendSpaceSample{.position = 2.0f, .clip = &g_jog, .speed = 1.5f});
    space.samples.push_back(BlendSpaceSample{.position = 6.0f, .clip = &g_run, .speed = 2.0f});
    return space;
}

bool TestValidSpaceIsAccepted()
{
    const BlendSpace1D space = MakeLocomotionSpace();
    return space.IsValid();
}

bool TestEmptySpaceIsInvalid()
{
    const BlendSpace1D space;
    usize lower = 0;
    usize upper = 0;
    f32 weight = 0.0f;
    return !space.IsValid() && !space.Evaluate(1.0f, lower, upper, weight);
}

bool TestNullClipIsInvalid()
{
    BlendSpace1D space = MakeLocomotionSpace();
    space.samples[1].clip = nullptr;
    return !space.IsValid();
}

bool TestUnsortedAxisIsInvalid()
{
    BlendSpace1D descending = MakeLocomotionSpace();
    std::swap(descending.samples[0].position, descending.samples[2].position);
    BlendSpace1D duplicated = MakeLocomotionSpace();
    duplicated.samples[1].position = duplicated.samples[0].position;
    return !descending.IsValid() && !duplicated.IsValid();
}

bool TestNonPositiveSpeedIsInvalid()
{
    BlendSpace1D space = MakeLocomotionSpace();
    space.samples[1].speed = 0.0f;
    if (space.IsValid()) return false;
    space.samples[1].speed = std::numeric_limits<f32>::quiet_NaN();
    return !space.IsValid();
}

bool TestValuesClampToTheAuthoredRange()
{
    const BlendSpace1D space = MakeLocomotionSpace();
    usize lower = 9;
    usize upper = 9;
    f32 weight = 9.0f;
    if (!space.Evaluate(-5.0f, lower, upper, weight)) return false;
    if (lower != 0 || upper != 0 || !Near(weight, 0.0f)) return false;
    if (!space.Evaluate(100.0f, lower, upper, weight)) return false;
    return lower == 2 && upper == 2 && Near(weight, 0.0f);
}

bool TestNaNAndInfinitiesClampLow()
{
    const BlendSpace1D space = MakeLocomotionSpace();
    usize lower = 9;
    usize upper = 9;
    f32 weight = 9.0f;
    if (!space.Evaluate(std::numeric_limits<f32>::quiet_NaN(), lower, upper, weight)) return false;
    if (lower != 0 || upper != 0 || !Near(weight, 0.0f)) return false;
    return space.Evaluate(-std::numeric_limits<f32>::infinity(), lower, upper, weight) &&
           lower == 0 && upper == 0;
}

bool TestExactSampleHasNoBlend()
{
    const BlendSpace1D space = MakeLocomotionSpace();
    usize lower = 9;
    usize upper = 9;
    f32 weight = 9.0f;
    if (!space.Evaluate(2.0f, lower, upper, weight)) return false;
    return lower == 1 && upper == 1 && Near(weight, 0.0f);
}

bool TestBetweenSamplesBlendsByDistance()
{
    const BlendSpace1D space = MakeLocomotionSpace();
    usize lower = 9;
    usize upper = 9;
    f32 weight = 9.0f;
    if (!space.Evaluate(1.0f, lower, upper, weight)) return false;
    if (lower != 0 || upper != 1 || !Near(weight, 0.5f)) return false;
    if (!space.Evaluate(4.0f, lower, upper, weight)) return false;
    return lower == 1 && upper == 2 && Near(weight, 0.5f);
}

bool TestSingleSampleSpaceAlwaysResolves()
{
    BlendSpace1D space;
    space.samples.push_back(BlendSpaceSample{.position = 3.0f, .clip = &g_walk});
    usize lower = 9;
    usize upper = 9;
    f32 weight = 9.0f;
    if (!space.IsValid() || !space.Evaluate(99.0f, lower, upper, weight)) return false;
    return lower == 0 && upper == 0 && Near(weight, 0.0f);
}

bool TestPhaseAdvancesAtTheBlendedRate()
{
    // One second of wall clock across a one-second clip is one full cycle.
    return Near(Concord::AdvanceBlendSpacePhase(0.0f, 1.0f, 1.0f, 1.0f), 0.0f) &&
           Near(Concord::AdvanceBlendSpacePhase(0.0f, 0.25f, 1.0f, 1.0f), 0.25f) &&
           // A doubled playback rate covers twice the phase in the same time.
           Near(Concord::AdvanceBlendSpacePhase(0.0f, 0.25f, 1.0f, 2.0f), 0.5f) &&
           // A clip twice as long advances half as far.
           Near(Concord::AdvanceBlendSpacePhase(0.0f, 0.25f, 2.0f, 1.0f), 0.125f);
}

bool TestPhaseWrapsIntoUnitRange()
{
    return Near(Concord::AdvanceBlendSpacePhase(0.75f, 0.5f, 1.0f, 1.0f), 0.25f) &&
           Near(Concord::AdvanceBlendSpacePhase(0.0f, 3.25f, 1.0f, 1.0f), 0.25f) &&
           Near(Concord::AdvanceBlendSpacePhase(0.9f, 0.0f, 1.0f, 1.0f), 0.9f);
}

bool TestDegenerateStepsLeaveThePhaseAlone()
{
    const f32 nan = std::numeric_limits<f32>::quiet_NaN();
    return Near(Concord::AdvanceBlendSpacePhase(0.4f, -1.0f, 1.0f, 1.0f), 0.4f) &&
           Near(Concord::AdvanceBlendSpacePhase(0.4f, nan, 1.0f, 1.0f), 0.4f) &&
           Near(Concord::AdvanceBlendSpacePhase(0.4f, 0.1f, 0.0f, 1.0f), 0.4f) &&
           Near(Concord::AdvanceBlendSpacePhase(0.4f, 0.1f, 1.0f, 0.0f), 0.4f) &&
           Near(Concord::AdvanceBlendSpacePhase(nan, 0.1f, 1.0f, 1.0f), 0.0f);
}

bool TestResetClearsThePlayhead()
{
    BlendSpaceState state;
    state.phase = 0.7f;
    const BlendSpace1D space = MakeLocomotionSpace();
    if (!Concord::ResetBlendSpace(space, state) || !Near(state.phase, 0.0f)) return false;
    const BlendSpace1D empty;
    state.phase = 0.3f;
    return !Concord::ResetBlendSpace(empty, state) && Near(state.phase, 0.0f);
}

bool TestSamplingAnInvalidSpaceIsRefused()
{
    const BlendSpace1D empty;
    BlendSpaceState state;
    Concord::Skeleton skeleton;
    Concord::SkeletonPose output;
    Concord::SkeletonPose scratch;
    return !Concord::SampleBlendSpace1D(empty, state, 1.0f, 0.016f, skeleton, output, scratch);
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"valid space is accepted", TestValidSpaceIsAccepted},
        {"empty space is invalid", TestEmptySpaceIsInvalid},
        {"null clip is invalid", TestNullClipIsInvalid},
        {"unsorted axis is invalid", TestUnsortedAxisIsInvalid},
        {"non-positive speed is invalid", TestNonPositiveSpeedIsInvalid},
        {"values clamp to the authored range", TestValuesClampToTheAuthoredRange},
        {"NaN and infinities clamp low", TestNaNAndInfinitiesClampLow},
        {"exact sample has no blend", TestExactSampleHasNoBlend},
        {"between samples blends by distance", TestBetweenSamplesBlendsByDistance},
        {"single-sample space always resolves", TestSingleSampleSpaceAlwaysResolves},
        {"phase advances at the blended rate", TestPhaseAdvancesAtTheBlendedRate},
        {"phase wraps into unit range", TestPhaseWrapsIntoUnitRange},
        {"degenerate steps leave the phase alone", TestDegenerateStepsLeaveThePhaseAlone},
        {"reset clears the playhead", TestResetClearsThePlayhead},
        {"sampling an invalid space is refused", TestSamplingAnInvalidSpaceIsRefused},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const Concord::u32 total = static_cast<Concord::u32>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " blend space cases passed\n";
    return failures == 0 ? 0 : 1;
}
