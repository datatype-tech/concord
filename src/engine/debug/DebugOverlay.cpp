// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/debug/DebugOverlay.h"

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

void DebugOverlay::Update(f32 deltaTime, usize entityCount, const RenderBackendStats& renderStats)
{
    m_lastFrameTime = deltaTime > kMinimumFrameTime ? deltaTime : 0.0f;
    m_averageFrameTime =
        m_averageFrameTime <= 0.0f ? m_lastFrameTime
                                   : m_averageFrameTime + (m_lastFrameTime - m_averageFrameTime) * kSmoothing;
    m_entityCount = entityCount;
    m_stats = renderStats;

    m_frame.visible = showDebugInfo;
    if (!showDebugInfo) {
        m_frame.lineCount = 0;
        return;
    }

    const f32 averageMs = m_averageFrameTime * 1000.0f;
    const f32 fps = m_averageFrameTime > 0.0f ? 1.0f / m_averageFrameTime : 0.0f;
    m_frame.lineCount = 0;
    FormatLine(m_frame, 0, "fps %5.1f  (%.2f ms avg)", fps, averageMs);
    FormatLine(m_frame, 1, "frame %.2f ms", m_lastFrameTime * 1000.0f);
    FormatLine(m_frame, 2, "render %ux%u  ray tracing %s", m_stats.width, m_stats.height,
               m_stats.rayTracingActive ? "on" : "off");
    FormatLine(m_frame, 3, "entities %llu  objects %u  lights %u",
               static_cast<unsigned long long>(m_entityCount), m_stats.visibleObjects,
               m_stats.lights);
}

const DebugOverlayFrame& DebugOverlay::Frame() const noexcept { return m_frame; }

f32 DebugOverlay::AverageFrameTime() const noexcept { return m_averageFrameTime; }

f32 DebugOverlay::LastFrameTime() const noexcept { return m_lastFrameTime; }

} // namespace Concord
