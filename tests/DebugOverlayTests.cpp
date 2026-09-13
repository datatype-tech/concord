// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/debug/DebugOverlay.h"

#include <cstdio>
#include <cstring>

namespace {

/** Feeds a steady frame rate and checks the overlay reports it. */
bool TestReportsMeasuredFrameRate()
{
    Concord::DebugOverlay overlay;
    overlay.showDebugInfo = true;
    const Concord::RenderBackendStats stats{.width = 1600,
                                            .height = 900,
                                            .visibleObjects = 7,
                                            .lights = 2,
                                            .rayTracingActive = false};
    for (Concord::u32 i = 0; i < 400; ++i) {
        overlay.Update(1.0f / 60.0f, 1.0f / 200.0f, 12, stats);
    }
    // The headline rate is the engine's own 200, not the paced 60. Reporting
    // the paced rate as "fps" is what made the readout look fake: under vsync
    // it is one divided by the refresh interval, a constant.
    const char* sceneLine = overlay.Frame().lines[0].text;
    if (std::strstr(sceneLine, "entities 12") == nullptr ||
        std::strstr(sceneLine, "objects 7") == nullptr ||
        std::strstr(sceneLine, "lights 2") == nullptr) {
        std::printf("scene line wrong: %s\n", sceneLine);
        return false;
    }
    const char* fpsLine = overlay.Frame().lines[1].text;
    if (std::strstr(fpsLine, "200") == nullptr || std::strstr(fpsLine, "fps") == nullptr) {
        std::printf("fps line wrong: %s\n", fpsLine);
        return false;
    }
    const Concord::f32 expectedMs = 1000.0f / 60.0f;
    const Concord::f32 averageMs = overlay.AverageFrameTime() * 1000.0f;
    if (averageMs < expectedMs - 0.5f || averageMs > expectedMs + 0.5f) {
        std::printf("average frame time off: %f\n", averageMs);
        return false;
    }
    if (overlay.AverageCpuTime() < 0.0049f || overlay.AverageCpuTime() > 0.0051f) {
        std::printf("average cpu time off: %f\n", overlay.AverageCpuTime());
        return false;
    }
    if (overlay.Frame().lineCount != 2) {
        std::printf("overlay line count wrong: %u\n", overlay.Frame().lineCount);
        return false;
    }
    return true;
}

/** Tracks a changing frame rate instead of latching onto the first sample. */
bool TestAverageFollowsNewRate()
{
    Concord::DebugOverlay overlay;
    overlay.showDebugInfo = true;
    const Concord::RenderBackendStats stats{};
    for (Concord::u32 i = 0; i < 200; ++i) {
        overlay.Update(1.0f / 120.0f, 1.0f / 120.0f, 1, stats);
    }
    for (Concord::u32 i = 0; i < 400; ++i) {
        overlay.Update(1.0f / 30.0f, 1.0f / 30.0f, 1, stats);
    }
    const Concord::f32 averageMs = overlay.AverageFrameTime() * 1000.0f;
    if (averageMs < 33.3f - 1.0f || averageMs > 33.3f + 1.0f) {
        std::printf("average did not follow the new rate: %f\n", averageMs);
        return false;
    }
    return true;
}

/** Hidden overlays carry no lines and toggling restores them immediately. */
bool TestHiddenOverlayStaysEmpty()
{
    Concord::DebugOverlay overlay;
    const Concord::RenderBackendStats stats{.width = 640, .height = 480};
    overlay.Update(1.0f / 60.0f, 1.0f / 60.0f, 5, stats);
    if (overlay.Frame().visible || overlay.Frame().lineCount != 0) {
        std::printf("hidden overlay produced lines\n");
        return false;
    }
    if (overlay.AverageFrameTime() <= 0.0f) {
        std::printf("hidden overlay stopped measuring\n");
        return false;
    }
    overlay.showDebugInfo = true;
    overlay.Update(1.0f / 60.0f, 1.0f / 60.0f, 5, stats);
    if (!overlay.Frame().visible || overlay.Frame().lineCount == 0) {
        std::printf("enabled overlay stayed hidden\n");
        return false;
    }
    return true;
}

} // namespace

int main()
{
    struct TestCase {
        const char* name;
        bool (*run)();
    };
    const TestCase tests[] = {
        {"measured frame rate", TestReportsMeasuredFrameRate},
        {"average follows new rate", TestAverageFollowsNewRate},
        {"hidden overlay stays empty", TestHiddenOverlayStaysEmpty},
    };
    for (const TestCase& test : tests) {
        const bool passed = test.run();
        std::printf("[%s] %s\n", passed ? "PASS" : "FAIL", test.name);
        if (!passed) {
            return 1;
        }
    }
    return 0;
}
