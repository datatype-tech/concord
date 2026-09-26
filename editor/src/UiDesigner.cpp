// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/UiDesigner.h"
#include "editor/UiDesignerState.h"
#include "editor/NativeDialogs.h"
#include "editor/ProjectDocument.h"
#include "editor/SceneDocument.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>

namespace Concord::Editor {

UiDesigner::UiDesigner() : m_impl(std::make_unique<Impl>()) {}
UiDesigner::~UiDesigner() = default;
void UiDesigner::SetProjectDirectory(const std::filesystem::path& directory) { SetProjectRoot(directory); }
void UiDesigner::SetProjectRoot(const std::filesystem::path& directory)
{
    if (m_impl->loaded && m_impl->Dirty() && directory != m_impl->project)
        throw std::runtime_error("Save the current interface before switching projects.");
    const auto root = directory.empty() ? directory : ResolveProjectPath(directory, "Concord.project").parent_path();
    if (m_impl->project != root && m_impl->loaded) {
        if (m_impl->Dirty()) throw std::runtime_error("Save the current interface before switching projects.");
        m_impl = std::make_unique<Impl>();
    }
    m_impl->project = root;
}
bool UiDesigner::Loaded() const { return m_impl->loaded; }
bool UiDesigner::Dirty() const { return m_impl->Dirty(); }
const std::filesystem::path& UiDesigner::Path() const { return m_impl->path; }
bool UiDesigner::Load(const std::filesystem::path& path)
{
    if (m_impl->loaded && m_impl->path == path) return true;
    m_impl->Request(Impl::Pending::Load, path);
    return m_impl->pending == Impl::Pending::None && m_impl->error.empty();
}
void UiDesigner::NewDocument() { m_impl->Request(Impl::Pending::New); }
bool UiDesigner::Save() { return m_impl->Save(); }
void UiDesigner::SaveRecovery(const std::filesystem::path& path) const
{
    if (!m_impl->Dirty()) return;
    const auto target = m_impl->ResolvePath(Utf8Text(path));
    std::filesystem::create_directories(target.parent_path());
    m_impl->document.Save(m_impl->ResolvePath(Utf8Text(target)));
}
void UiDesigner::Undo() { m_impl->Undo(false); }
void UiDesigner::Redo() { m_impl->Undo(true); }
void UiDesigner::Draw()
{
    m_impl->editSubmitted = false;
    m_impl->Hierarchy();
    m_impl->Canvas();
    m_impl->Properties();
    m_impl->Dialogs();
    if (!m_impl->editSubmitted && !m_impl->dragging) m_impl->FinishEdit();
}

bool UiDesigner::Impl::Dirty() const { return loaded && document.Serialize() != saved; }
int UiDesigner::Impl::SelectedIndex() const
{
    for (int index = 0; index < static_cast<int>(document.elements.size()); ++index)
        if (document.elements[index].id == selected) return index;
    return -1;
}
bool UiDesigner::Impl::DescendantOf(const std::string& id, const std::string& parent) const
{
    std::string cursor = id;
    for (size_t depth = 0; depth <= document.elements.size() && !cursor.empty(); ++depth) {
        if (cursor == parent) return true;
        auto found = std::find_if(document.elements.begin(), document.elements.end(), [&](const auto& element) { return element.id == cursor; });
        if (found == document.elements.end()) return false;
        cursor = found->parent;
    }
    return false;
}
std::string UiDesigner::Impl::UniqueId(const std::string& stem) const
{
    for (size_t count = 1;; ++count) {
        const auto candidate = stem.substr(0, 80) + std::to_string(count);
        if (std::none_of(document.elements.begin(), document.elements.end(), [&](const auto& element) { return element.id == candidate; }))
            return candidate;
    }
}
std::filesystem::path UiDesigner::Impl::ResolvePath(const std::string& name) const
{
    auto file = Utf8Path(name);
    if (file.empty()) throw std::runtime_error("Choose a .yu filename.");
    if (file.extension().empty()) file += L".yu";
    if (_wcsicmp(file.extension().c_str(), L".yu") != 0) throw std::runtime_error("UI documents use the .yu extension.");
    if (project.empty()) throw std::runtime_error("Open a project before opening or saving an interface.");
    if (file.is_absolute()) file = file.lexically_relative(std::filesystem::absolute(project));
    return ResolveProjectPath(project, Utf8Text(file));
}
void UiDesigner::Impl::Request(Pending action, const std::filesystem::path& file)
{
    FinishEdit();
    pending = action;
    pendingPath = file;
    error.clear();
    if (Dirty()) askUnsaved = true;
    else ApplyPending();
}
void UiDesigner::Impl::ApplyPending()
{
    try {
        UiDocument replacement;
        std::filesystem::path replacementPath;
        std::string replacementSource;
        if (pending == Pending::Load) {
            replacementPath = ResolvePath(Utf8Text(pendingPath));
            replacementSource = ReadText(replacementPath);
            replacement.Parse(replacementSource);
        } else if (pending == Pending::New) {
            UiElement panel;
            panel.id = "Panel1"; panel.kind = UiElementKind::Panel; panel.text.clear();
            panel.position = {48, 48}; panel.size = {420, 290}; panel.background = COLOR_RGB(27, 32, 45); panel.rounding = 16;
            UiElement label;
            label.id = "Title1"; label.parent = panel.id; label.kind = UiElementKind::Label;
            label.text = "My interface"; label.position = {28, 24}; label.size = {340, 48}; label.fontScale = 1.5f;
            UiElement button;
            button.id = "Button1"; button.parent = panel.id; button.text = "Play";
            button.position = {28, 110}; button.size = {200, 48}; button.background = COLOR_RGB(83, 98, 223);
            replacement.elements = {panel, label, button};
        } else return;
        replacement.Validate();
        document = std::move(replacement);
        path = replacementPath;
        diskSource = std::move(replacementSource);
        loaded = true;
        saved = pending == Pending::Load ? document.Serialize() : std::string{};
        selected = document.elements.empty() ? "" : document.elements.front().id;
        undo.clear(); redo.clear(); editBefore.clear(); dragging = false;
        status = path.empty() ? "Untitled interface" : "Opened " + Utf8Text(path.filename());
        error.clear(); pending = Pending::None; pendingPath.clear(); askUnsaved = false; afterSave = false;
    } catch (const std::exception& exception) {
        error = exception.what(); pending = Pending::None; askUnsaved = false;
    }
}
bool UiDesigner::Impl::SavePath(const std::filesystem::path& file)
{
    try {
        const auto target = ResolvePath(Utf8Text(file));
        if (target == path && (!std::filesystem::is_regular_file(target) || ReadText(target) != diskSource))
            throw std::runtime_error("This UI file changed outside the editor. Save as a new file to preserve both versions.");
        std::filesystem::create_directories(target.parent_path());
        document.Save(ResolvePath(Utf8Text(target)));
        path = target; saved = document.Serialize(); diskSource = saved;
        status = "Saved " + Utf8Text(path.filename()); error.clear();
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}
bool UiDesigner::Impl::Save()
{
    if (!loaded) return true;
    FinishEdit();
    if (!path.empty()) return SavePath(path);
    saveDialog = true;
    std::snprintf(fileName, sizeof(fileName), "%s", "UI/Interface.yu");
    return false;
}
void UiDesigner::Impl::Checkpoint(const std::string& before)
{
    if (before.empty() || before == document.Serialize()) return;
    if (undo.empty() || undo.back() != before) undo.push_back(before);
    if (undo.size() > 64) undo.pop_front();
    redo.clear();
}
void UiDesigner::Impl::FinishEdit()
{
    if (!editBefore.empty()) { Checkpoint(editBefore); editBefore.clear(); }
    if (dragging) { Checkpoint(dragBefore); dragBefore.clear(); dragging = false; }
}
void UiDesigner::Impl::TrackItem(const std::string& before, bool changed)
{
    if (changed) {
        try { static_cast<void>(document.Serialize()); }
        catch (const std::exception& exception) {
            UiDocument previous; previous.Parse(before);
            document.referenceSize = previous.referenceSize;
            for (size_t index = 0; index < document.elements.size(); ++index) document.elements[index] = previous.elements[index];
            error = exception.what(); changed = false;
        }
    }
    if (ImGui::IsItemActivated()) { FinishEdit(); editBefore = before; }
    if (ImGui::IsItemActive() || ImGui::IsItemDeactivated()) editSubmitted = true;
    if (ImGui::IsItemDeactivated()) FinishEdit();
    else if (changed && !ImGui::IsItemActive()) Checkpoint(before);
}
void UiDesigner::Impl::Select(const std::string& id) { FinishEdit(); selected = id; }
void UiDesigner::Impl::Undo(bool forward)
{
    if (!loaded) return;
    FinishEdit();
    auto& source = forward ? redo : undo;
    auto& destination = forward ? undo : redo;
    if (source.empty()) return;
    destination.push_back(document.Serialize());
    document.Parse(source.back()); source.pop_back();
    if (SelectedIndex() < 0) selected.clear();
    dragging = false;
}
void UiDesigner::Impl::Add(UiElementKind kind)
{
    if (!loaded) return;
    if (document.elements.size() >= 2048) { error = "A UI document supports up to 2048 elements."; return; }
    FinishEdit(); const auto before = document.Serialize();
    static const char* names[]{"Panel", "Label", "Button", "Checkbox", "Slider", "Input", "Progress"};
    UiElement element;
    element.kind = kind; element.id = UniqueId(names[static_cast<int>(kind)]); element.text = names[static_cast<int>(kind)];
    element.position = {32, 32};
    const int index = SelectedIndex();
    if (index >= 0) element.parent = document.elements[index].kind == UiElementKind::Panel ? selected : document.elements[index].parent;
    if (kind == UiElementKind::Panel) { element.size = {320, 220}; element.text.clear(); }
    if (kind == UiElementKind::Label) element.size = {200, 36};
    if (kind == UiElementKind::Progress || kind == UiElementKind::Slider) element.value = 0.5f;
    auto replacement = document;
    replacement.elements.push_back(element);
    try { static_cast<void>(replacement.Serialize()); }
    catch (const std::exception& exception) { error = exception.what(); return; }
    selected = element.id; document = std::move(replacement); Checkpoint(before);
}
void UiDesigner::Impl::Delete()
{
    if (SelectedIndex() < 0) return;
    FinishEdit(); const auto before = document.Serialize();
    std::vector<std::string> removed;
    for (const auto& element : document.elements) if (DescendantOf(element.id, selected)) removed.push_back(element.id);
    std::erase_if(document.elements, [&](const auto& element) { return std::find(removed.begin(), removed.end(), element.id) != removed.end(); });
    selected.clear(); Checkpoint(before);
}
void UiDesigner::Impl::Duplicate()
{
    if (SelectedIndex() < 0) return;
    FinishEdit(); const auto before = document.Serialize();
    const auto root = selected;
    std::vector<UiElement> copies;
    for (const auto& element : document.elements) if (DescendantOf(element.id, root)) copies.push_back(element);
    if (document.elements.size() + copies.size() > 2048) { error = "Duplicating this subtree would exceed 2048 elements."; return; }
    std::map<std::string, std::string> names;
    for (auto& element : copies) {
        const auto original = element.id;
        element.id = UniqueId(original + "Copy"); names.emplace(original, element.id);
        document.elements.push_back(element);
    }
    const auto offset = document.elements.size() - copies.size();
    for (size_t index = offset; index < document.elements.size(); ++index) {
        auto& element = document.elements[index];
        if (names.contains(element.parent)) element.parent = names.at(element.parent);
        if (element.id == names.at(root)) { element.position.x += 16; element.position.y += 16; selected = element.id; }
    }
    try { static_cast<void>(document.Serialize()); }
    catch (const std::exception& exception) { document.Parse(before); selected = root; error = exception.what(); return; }
    Checkpoint(before);
}
void UiDesigner::Impl::Dialogs()
{
    if (askUnsaved) { ImGui::OpenPopup("Unsaved interface"); askUnsaved = false; }
    if (ImGui::BeginPopupModal("Unsaved interface", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save the current interface before switching?");
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        if (ImGui::Button("Save and continue")) {
            if (Save()) { ApplyPending(); ImGui::CloseCurrentPopup(); }
            else if (saveDialog) { afterSave = true; ImGui::CloseCurrentPopup(); }
        }
        ImGui::SameLine(); if (ImGui::Button("Discard")) { ApplyPending(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) { pending = Pending::None; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (saveDialog) { ImGui::OpenPopup("Save interface"); saveDialog = false; overwrite = false; }
    if (ImGui::BeginPopupModal("Save interface", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Filename inside the current project");
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 26);
        if (ImGui::InputText("##uiFilename", fileName, sizeof(fileName))) overwrite = false;
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        if (overwrite) ImGui::TextColored({1,0.7f,0.3f,1}, "This file exists. Replace its contents?");
        if (ImGui::Button(overwrite ? "Replace file" : "Save")) {
            try {
                const auto file = ResolvePath(fileName);
                if (std::filesystem::exists(file) && file != path && !overwrite) overwrite = true;
                else if (SavePath(file)) {
                    ImGui::CloseCurrentPopup();
                    if (afterSave) ApplyPending();
                }
            } catch (const std::exception& exception) { error = exception.what(); }
        }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) { afterSave = false; pending = Pending::None; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if (openDialog) { ImGui::OpenPopup("Open interface"); openDialog = false; }
    if (ImGui::BeginPopupModal("Open interface", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(".yu path inside the current project");
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 26);
        ImGui::InputText("##openUiFilename", fileName, sizeof(fileName));
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        if (ImGui::Button("Open")) {
            try { const auto file = ResolvePath(fileName); Request(Pending::Load, file); ImGui::CloseCurrentPopup(); }
            catch (const std::exception& exception) { error = exception.what(); }
        }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace Concord::Editor
