// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_DEBUGOVERLAY_H
#define CONCORD_DEBUGOVERLAY_H

#include "Concord/CExport.h"
#include "engine/debug/DebugOverlayFrame.h"
#include "engine/render/IRenderBackend.h"

namespace Concord {

/**
 * Standalone debug component that turns per-frame measurements into the
 * text lines shown in the top-right corner of the window.
 *
 * Every displayed number is measured: frame times come from the Game loop's
 * steady clock, entity and light counts from the live scene and renderer.
 * The component only formats; drawing the text is the render backend's job,
 * so the cost when `showDebugInfo` is false is a few float operations per
 * frame.
 */
class CENGINE_API DebugOverlay {
public:
    /** When false the overlay is not submitted to the renderer at all. */
    bool showDebugInfo = false;

    /**
     * Records one frame and refreshes the overlay text.
     *
     * @param deltaTime Seconds elapsed since the previous Update call.
     * @param entityCount Entities alive in the scene being rendered.
     * @param renderStats What the backend actually drew last frame.
     */
    void Update(f32 deltaTime, usize entityCount, const RenderBackendStats& renderStats);

    /** The frame to submit to the render backend; valid until next Update. */
    [[nodiscard]] const DebugOverlayFrame& Frame() const noexcept;

    /** Exponentially smoothed frame time in seconds; zero before Update. */
    [[nodiscard]] f32 AverageFrameTime() const noexcept;

    /** Frame time most recently passed to Update, in seconds. */
    [[nodiscard]] f32 LastFrameTime() const noexcept;

private:
    DebugOverlayFrame m_frame{};
    f32 m_averageFrameTime = 0.0f;
    f32 m_lastFrameTime = 0.0f;
    usize m_entityCount = 0;
    RenderBackendStats m_stats{};
};

} // namespace Concord

#endif // CONCORD_DEBUGOVERLAY_H
