// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORUIDESIGNERSTATE_H
#define CONCORD_EDITORUIDESIGNERSTATE_H

#include "editor/UiDesigner.h"
#include <Concord/CUiDocument.h>
#include <Concord/CUiToolkit.h>

#include <deque>
#include <string>

namespace Concord::Editor {

struct UiDesigner::Impl {
    enum class Pending { None, New, Load };
    UiDocument document;
    std::filesystem::path project, path, pendingPath;
    std::string selected, saved, diskSource, error, status = "Create or open a .yu interface";
    std::string editBefore, dragBefore;
    std::deque<std::string> undo, redo;
    Pending pending = Pending::None;
    bool loaded = false, askUnsaved = false, saveDialog = false, openDialog = false;
    bool afterSave = false, overwrite = false, showGrid = true, snap = true, fit = true;
    bool editSubmitted = false, dragging = false, resizing = false;
    float zoom = 1.0f, grid = 8.0f;
    ImVec2 dragMouse{};
    UiElement dragElement;
    char fileName[2048] = "UI/Interface.yu";
    char rename[256]{};

    bool Dirty() const;
    int SelectedIndex() const;
    bool DescendantOf(const std::string& id, const std::string& parent) const;
    std::string UniqueId(const std::string& stem) const;
    std::filesystem::path ResolvePath(const std::string& name) const;
    void ApplyPending();
    void Request(Pending action, const std::filesystem::path& file = {});
    bool Save();
    bool SavePath(const std::filesystem::path& file);
    void Dialogs();
    void Select(const std::string& id);
    void Checkpoint(const std::string& before);
    void FinishEdit();
    void TrackItem(const std::string& before, bool changed);
    void Undo(bool forward);
    void Add(UiElementKind kind);
    void Duplicate();
    void Delete();
    void Canvas();
    void Hierarchy();
    void Properties();
};

} // namespace Concord::Editor

#endif // CONCORD_EDITORUIDESIGNERSTATE_H
