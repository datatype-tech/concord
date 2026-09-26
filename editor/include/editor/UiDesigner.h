// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORUIDESIGNER_H
#define CONCORD_EDITORUIDESIGNER_H

#include <filesystem>
#include <memory>

namespace Concord::Editor {

/** Visual .yu authoring using the engine's document renderer and layout rules. */
class UiDesigner {
public:
    UiDesigner();
    ~UiDesigner();
    UiDesigner(const UiDesigner&) = delete;
    UiDesigner& operator=(const UiDesigner&) = delete;

    /** Sets the default directory for new and opened UI documents. */
    void SetProjectDirectory(const std::filesystem::path& directory);
    /** Alias used by the owning project workspace. */
    void SetProjectRoot(const std::filesystem::path& directory);
    /** Opens a document; pending dirty-file decisions are presented by Draw(). */
    bool Load(const std::filesystem::path& path);
    /** Saves the active document, or presents a filename dialog for a new one. */
    bool Save();
    /** Writes unsaved content to a recovery file without changing the current document. */
    void SaveRecovery(const std::filesystem::path& path) const;
    /** Creates an untitled document after resolving existing unsaved changes. */
    void NewDocument();
    /** Whether the current document differs from its saved revision. */
    [[nodiscard]] bool Dirty() const;
    [[nodiscard]] bool Loaded() const;
    [[nodiscard]] const std::filesystem::path& Path() const;
    /** Draws UI Canvas, UI Elements, and UI Properties docking panels. */
    void Draw();
    void Undo();
    void Redo();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Concord::Editor

#endif // CONCORD_EDITORUIDESIGNER_H
