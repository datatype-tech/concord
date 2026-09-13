// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/debug/DebugOverlay.h"

#include <algorithm>
#include <cmath>

#include <cstdarg>
#include <cstdio>

namespace Concord {
namespace {

/** Fraction of each new sample folded into the running average. */
constexpr f32 kSmoothing = 0.05f;
/** Frame time below which a sample is treated as noise and dropped. */
constexpr f32 kMinimumFrameTime = 0.0001f;

/** Formats one overlay line, truncating silently at the fixed capacity. */
void FormatLine(DebugOverlayFrame& frame, u32 index, const char* format, ...)
{
    if (index >= kDebugOverlayMaxLines) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(frame.lines[index].text, kDebugOverlayLineLength, format, arguments);
    va_end(arguments);
    if (index + 1 > frame.lineCount) {
        frame.lineCount = index + 1;
    }
}

} // namespace

void DebugOverlay::Update(f32 frameSeconds, f32 cpuSeconds, usize entityCount,
                          const RenderBackendStats& renderStats)
{
    m_lastFrameTime = frameSeconds > kMinimumFrameTime ? frameSeconds : 0.0f;
    m_averageFrameTime =
        m_averageFrameTime <= 0.0f ? m_lastFrameTime
                                   : m_averageFrameTime + (m_lastFrameTime - m_averageFrameTime) * kSmoothing;

    const f32 lastCpu = cpuSeconds > kMinimumFrameTime ? cpuSeconds : 0.0f;
    m_averageCpuTime = m_averageCpuTime <= 0.0f
                           ? lastCpu
                           : m_averageCpuTime + (lastCpu - m_averageCpuTime) * kSmoothing;
    m_entityCount = entityCount;
    m_stats = renderStats;

    m_frame.visible = showDebugInfo;
    if (!showDebugInfo) {
        m_frame.lineCount = 0;
        return;
    }

    // Engine cost, not the paced present. Under vsync the full frame time is
    // the refresh interval, and printing that as fps is a constant.
    const f32 engineFps = m_averageCpuTime > 0.0f ? 1.0f / m_averageCpuTime : 0.0f;
    m_frame.lineCount = 0;
    FormatLine(m_frame, 0, "entities %llu  objects %u  lights %u",
               static_cast<unsigned long long>(m_entityCount), m_stats.visibleObjects,
               m_stats.lights);
    FormatLine(m_frame, 1, "%.0f fps", engineFps);
}

const DebugOverlayFrame& DebugOverlay::Frame() const noexcept { return m_frame; }

f32 DebugOverlay::AverageFrameTime() const noexcept { return m_averageFrameTime; }

f32 DebugOverlay::AverageCpuTime() const noexcept { return m_averageCpuTime; }

f32 DebugOverlay::LastFrameTime() const noexcept { return m_lastFrameTime; }

} // namespace Concord
