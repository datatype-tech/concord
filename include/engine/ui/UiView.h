// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_UIVIEW_H
#define CONCORD_UIVIEW_H

#include "Concord/CExport.h"
#include "engine/ui/document/UiDocument.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace Concord {
class Window;

/**
 * A .yu document the game can show and close.
 *
 * Construct it from a file path and keep the object alive for as long as the
 * interface should exist. Game::Run draws every open view after the frame's
 * OnUi callback, so ConcordScript does not call Draw itself. The toolkit must
 * be enabled. Events() is what the previous frame reported; read it from update.
 *
 * ConcordScript:
 * use Concord.CUi;
 * Concord::UiView& Menu() { static Concord::UiView view("UI/Menu.yu"); return view; }
 * @startup { Menu().SetPlace(Concord::UiPlace::Center); Menu().Show(); }
 * @update { if (!Menu().Events().empty()) Menu().Close(); }
 */
class CENGINE_API UiView {
public:
    /** Loads and validates the document. Throws if the file cannot be read. */
    explicit UiView(std::filesystem::path path);
    ~UiView();
    UiView(const UiView&) = delete;
    UiView& operator=(const UiView&) = delete;
    UiView(UiView&&) noexcept;
    UiView& operator=(UiView&&) noexcept;

    /** Includes this view in subsequent frames. */
    void Show();
    /** Removes this view from subsequent frames. */
    void Close();
    [[nodiscard]] bool IsOpen() const noexcept;
    /** Overrides the slot stored in the file until the next SetPlace or SetRect. */
    void SetPlace(UiPlace place);
    /** Selects a free rectangle in reference pixels and shows that slot. */
    void SetRect(Vec2 position, Vec2 size);
    /** Clicks and value edits from the previous presented frame. */
    [[nodiscard]] const std::vector<UiEvent>& Events() const;

    /** Draws open views into the current ImGui frame. Game::Run calls this. */
    static void Present(Window& window);

private:
    struct Impl;
    static std::vector<Impl*>& Views();
    std::unique_ptr<Impl> m_impl;
};
}
#endif
