// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_UITOOLKIT_H
#define CONCORD_UITOOLKIT_H
#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/ui/UiAppearance.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <memory>

namespace Concord {
class Window;
/** Optional docking UI context. SDL and Vulkan remain behind the engine boundary. */
class CENGINE_API UiToolkit {
public:
    UiToolkit();
    ~UiToolkit();
    UiToolkit(const UiToolkit&) = delete;
    UiToolkit& operator=(const UiToolkit&) = delete;
    /** Attaches platform input and a scalable system font to the engine window. */
    bool Init(Window& window);
    /** Releases platform state after the renderer has released its UI resources. */
    void Shutdown();
    /** Starts the optional UI frame; Game calls this before OnUpdate. */
    void Begin();
    /** Finalizes geometry; Game calls this after OnUpdate. */
    void End();
    /** Makes this context current before backend work. */
    void Activate() const;
    /** Applies a Hello ImGui theme and scale at the next frame boundary. */
    void SetAppearance(const UiAppearance& appearance);
    /** Returns the requested appearance, including changes pending the next frame. */
    [[nodiscard]] UiAppearance Appearance() const noexcept;
    /** Requests the next frame's offscreen scene size and marks this viewport visible. */
    void SetSceneViewport(u32 width, u32 height) noexcept;
    /** Opaque GPU texture for ImGui::Image, valid for the current UI frame. */
    [[nodiscard]] ImTextureID SceneTexture() const noexcept;
    [[nodiscard]] u32 ViewportWidth() const noexcept;
    [[nodiscard]] u32 ViewportHeight() const noexcept;
    [[nodiscard]] bool SceneVisible() const noexcept;
    /** Renderer bridge; no graphics API types cross into Runtime. */
    void SetSceneTexture(u64 texture) noexcept;
    /** Completed draw data, owned by this context until the next Begin. */
    [[nodiscard]] void* DrawData() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
