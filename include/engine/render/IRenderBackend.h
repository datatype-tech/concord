// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_IRENDERBACKEND_H
#define CONCORD_IRENDERBACKEND_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/debug/DebugOverlayFrame.h"

namespace Concord {

class Scene;
class Window;

/** Parameters steering backend bring-up; defaults keep every feature on. */
struct RenderBackendInit {
    /** Requests the validation layers when the build carries them. */
    bool enableValidation = true;
    /** Requests ray-tracing resources when the device and shaders allow. */
    bool enableRayTracing = true;
};

/** What the backend actually rendered in its most recent frame. */
struct RenderBackendStats {
    u32 width = 0;
    u32 height = 0;
    u32 visibleObjects = 0;
    u32 lights = 0;
    /** True when the last frame was produced by the ray-tracing path. */
    bool rayTracingActive = false;
};

/**
 * Abstract graphics backend.
 *
 * Implementations follow a fixed lifecycle (AGENTS.md §5):
 * Init() -> zero or more BeginFrame()/DrawScene()/EndFrame() triples ->
 * Shutdown(). Recreating the swapchain after a resize is the backend's own
 * business and never surfaces to callers.
 */
class CENGINE_API IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    /**
     * Brings up the device and swapchain for `window`.
     *
     * @return False when no usable device or surface could be acquired.
     */
    virtual bool Init(Window& window, const RenderBackendInit& init) = 0;

    /** Tears everything down; safe to call even when Init() failed. */
    virtual void Shutdown() = 0;

    /**
     * Acquires the next swapchain image and opens the frame's command buffer.
     *
     * @return False when the frame must be skipped, e.g. while the window is
     *         minimized or the swapchain is being rebuilt.
     */
    virtual bool BeginFrame() = 0;

    /** Records the draw commands for `scene` into the open command buffer. */
    virtual void DrawScene(const Scene& scene) = 0;

    /** Submits the command buffer and presents the acquired image. */
    virtual void EndFrame() = 0;

    /** Blocks until the device has finished all outstanding work. */
    virtual void WaitIdle() = 0;

    /**
     * Supplies the overlay text drawn on top of the scene, or nullptr to
     * disable it. The pointed-to frame must stay valid and be refreshed once
     * per frame, before BeginFrame(); reading it happens during DrawScene.
     */
    virtual void SetDebugOverlay(const DebugOverlayFrame* overlay) = 0;

    /** Statistics of the most recently completed frame; zeroed before one. */
    [[nodiscard]] virtual RenderBackendStats LastFrameStats() const = 0;
};

} // namespace Concord

#endif // CONCORD_IRENDERBACKEND_H