// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/UiDesignerState.h"
#include "editor/Design.h"
#include "editor/NativeDialogs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Concord::Editor {
namespace {
bool EditString(const char* label, std::string& text, size_t limit, bool multiline = false)
{
    std::vector<char> buffer(limit + 1, 0);
    std::copy_n(text.data(), std::min(text.size(), limit), buffer.data());
    const bool changed = multiline
        ? ImGui::InputTextMultiline(label, buffer.data(), buffer.size(), {0, ImGui::GetTextLineHeightWithSpacing() * 3})
        : ImGui::InputText(label, buffer.data(), buffer.size());
    if (changed) text = buffer.data();
    return changed;
}
bool EditColor(const char* label, ColorRGBA& color)
{
    float value[]{ColorR(color) / 255.0f, ColorG(color) / 255.0f, ColorB(color) / 255.0f, ColorA(color) / 255.0f};
    const bool changed = ImGui::ColorEdit4(label, value, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    if (changed) {
        auto channel = [](float component) { return static_cast<u8>(std::lround(std::clamp(component, 0.0f, 1.0f) * 255)); };
        color = MakeColor(channel(value[0]), channel(value[1]), channel(value[2]), channel(value[3]));
    }
    return changed;
}
}

void UiDesigner::Impl::Properties()
{
    if (ImGui::Begin("UI Properties")) {
        if (!loaded) ImGui::TextWrapped("Select an interface file to edit its properties.");
        else {
            const auto before = document.Serialize();
            const int index = SelectedIndex();
            ImGui::PushItemWidth(-1);
            if (index < 0) {
                Design::Eyebrow("DOCUMENT");
                ImGui::TextWrapped("%s", path.empty() ? "Untitled.yu" : Utf8Text(path.filename()).c_str());
                ImGui::Spacing(); ImGui::TextDisabled("Reference resolution");
                const bool changed = ImGui::DragFloat2("##uiResolution", &document.referenceSize.x, 1, 1, 100000, "%.0f", ImGuiSliderFlags_AlwaysClamp);
                TrackItem(before, changed);
                if (changed) fit = true;
                if (ImGui::Button("1280 x 720")) { FinishEdit(); document.referenceSize = {1280, 720}; Checkpoint(before); fit = true; }
                ImGui::SameLine(); if (ImGui::Button("1920 x 1080")) { FinishEdit(); document.referenceSize = {1920, 1080}; Checkpoint(before); fit = true; }
                if (ImGui::Button("720 x 1280")) { FinishEdit(); document.referenceSize = {720, 1280}; Checkpoint(before); fit = true; }
                ImGui::SeparatorText("Canvas tools");
                ImGui::Checkbox("Grid", &showGrid); ImGui::Checkbox("Snap to grid", &snap);
                ImGui::SliderFloat("Grid spacing", &grid, 1, 64, "%.0f px", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Separator();
                if (ImGui::Button("Save as...")) {
                    std::snprintf(fileName, sizeof(fileName), "%s", path.empty() ? "UI/Interface.yu" : Utf8Text(path.lexically_relative(project)).c_str());
                    saveDialog = true; error.clear();
                }
                ImGui::TextWrapped("Coordinates use reference pixels. Runtime layout scales uniformly to the host window; anchors follow the parent panel.");
            } else {
                auto& element = document.elements[index];
                Design::Eyebrow("ELEMENT");
                ImGui::TextWrapped("%s", element.id.c_str());
                if (ImGui::SmallButton("Rename ID")) {
                    std::snprintf(rename, sizeof(rename), "%s", element.id.c_str());
                    ImGui::OpenPopup("Rename UI element"); error.clear();
                }
                bool changed = ImGui::Checkbox("Visible", &element.visible); TrackItem(before, changed);
                ImGui::SameLine(); changed = ImGui::Checkbox("Enabled", &element.enabled); TrackItem(before, changed);
                ImGui::SeparatorText("Content");
                ImGui::TextDisabled("Control type");
                const char* kinds[]{"Panel", "Label", "Button", "Checkbox", "Slider", "Text input", "Progress"};
                int kind = static_cast<int>(element.kind);
                if (ImGui::Combo("##uiKind", &kind, kinds, 7)) {
                    FinishEdit(); const auto oldKind = element.kind; const auto oldAction = element.action;
                    element.kind = static_cast<UiElementKind>(kind);
                    if (element.kind != UiElementKind::Button) element.action = UiAction::None;
                    try { document.Validate(); Checkpoint(before); }
                    catch (const std::exception& exception) { element.kind = oldKind; element.action = oldAction; error = exception.what(); }
                }
                ImGui::TextDisabled(element.kind == UiElementKind::TextInput ? "Placeholder" : "Text");
                changed = EditString("##uiText", element.text, 16384, element.kind == UiElementKind::Label); TrackItem(before, changed);
                if (element.kind == UiElementKind::TextInput) {
                    ImGui::TextDisabled("Initial input value");
                    changed = EditString("##uiInitialInput", element.input, 4096); TrackItem(before, changed);
                }
                if (element.kind == UiElementKind::Checkbox) {
                    bool checked = element.value >= 0.5f;
                    changed = ImGui::Checkbox("Initially checked", &checked);
                    if (changed) element.value = checked ? 1.0f : 0.0f;
                    TrackItem(before, changed);
                }
                if (element.kind == UiElementKind::Slider || element.kind == UiElementKind::Progress) {
                    ImGui::TextDisabled("Initial value");
                    changed = ImGui::SliderFloat("##uiValue", &element.value, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp); TrackItem(before, changed);
                }
                ImGui::SeparatorText("Layout");
                ImGui::TextDisabled("Parent panel");
                if (ImGui::BeginCombo("##uiParent", element.parent.empty() ? "Document canvas" : element.parent.c_str())) {
                    auto chooseParent = [&](const std::string& parent, const char* label, const UiElementLayout* parentRect) {
                        if (!ImGui::Selectable(label, element.parent == parent)) return;
                        FinishEdit();
                        const auto currentLayout = document.ResolveLayout({.size = document.referenceSize});
                        const auto oldParent = element.parent; const auto oldPosition = element.position;
                        const Vec2 parentPosition = parentRect ? parentRect->position : Vec2{};
                        const Vec2 parentSize = parentRect ? parentRect->size : document.referenceSize;
                        element.parent = parent;
                        element.position = {currentLayout[index].position.x - parentPosition.x - element.anchor.x * (parentSize.x - element.size.x),
                                            currentLayout[index].position.y - parentPosition.y - element.anchor.y * (parentSize.y - element.size.y)};
                        try { document.Validate(); Checkpoint(before); }
                        catch (const std::exception& exception) { element.parent = oldParent; element.position = oldPosition; error = exception.what(); }
                    };
                    chooseParent("", "Document canvas", nullptr);
                    const auto layout = document.ResolveLayout({.size = document.referenceSize});
                    for (size_t parentIndex = 0; parentIndex < document.elements.size(); ++parentIndex) {
                        const auto& parent = document.elements[parentIndex];
                        if (parent.kind == UiElementKind::Panel && !DescendantOf(parent.id, element.id))
                            chooseParent(parent.id, parent.id.c_str(), &layout[parentIndex]);
                    }
                    ImGui::EndCombo();
                }
                ImGui::TextDisabled("Position");
                changed = ImGui::DragFloat2("##uiPosition", &element.position.x, 1, -1000000, 1000000, "%.1f", ImGuiSliderFlags_AlwaysClamp); TrackItem(before, changed);
                ImGui::TextDisabled("Size");
                changed = ImGui::DragFloat2("##uiSize", &element.size.x, 1, 1, 100000, "%.1f", ImGuiSliderFlags_AlwaysClamp); TrackItem(before, changed);
                ImGui::TextDisabled("Anchor (0 = start, 1 = end)");
                changed = ImGui::SliderFloat2("##uiAnchor", &element.anchor.x, 0, 1, "%.2f", ImGuiSliderFlags_AlwaysClamp); TrackItem(before, changed);
                if (ImGui::SmallButton("Top left")) { FinishEdit(); element.anchor = {0, 0}; Checkpoint(before); }
                ImGui::SameLine(); if (ImGui::SmallButton("Center")) { FinishEdit(); element.anchor = {0.5f, 0.5f}; element.position = {}; Checkpoint(before); }
                ImGui::SameLine(); if (ImGui::SmallButton("Bottom right")) { FinishEdit(); element.anchor = {1, 1}; Checkpoint(before); }
                ImGui::SeparatorText("Style");
                changed = EditColor("Text / foreground", element.color); TrackItem(before, changed);
                changed = EditColor("Background", element.background); TrackItem(before, changed);
                ImGui::TextDisabled("Corner radius");
                changed = ImGui::DragFloat("##uiRounding", &element.rounding, 0.25f, 0, 4096, "%.1f", ImGuiSliderFlags_AlwaysClamp); TrackItem(before, changed);
                ImGui::TextDisabled("Font scale");
                changed = ImGui::SliderFloat("##uiFontScale", &element.fontScale, 0.25f, 8, "%.2fx", ImGuiSliderFlags_AlwaysClamp); TrackItem(before, changed);
                if (element.kind == UiElementKind::Button) {
                    ImGui::SeparatorText("On click");
                    const char* actions[]{"Custom event only", "Minimize window", "Maximize / restore", "Toggle fullscreen", "Close window"};
                    int action = static_cast<int>(element.action);
                    changed = ImGui::Combo("##uiAction", &action, actions, 5);
                    if (changed) element.action = static_cast<UiAction>(action);
                    TrackItem(before, changed);
                    ImGui::TextWrapped("Window actions execute in the game. The designer preview is always non-interactive.");
                }
                if (ImGui::BeginPopupModal("Rename UI element", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextUnformatted("Stable element ID");
                    ImGui::InputText("##uiRename", rename, sizeof(rename));
                    if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
                    if (ImGui::Button("Rename")) {
                        try {
                            auto replacement = document;
                            replacement.elements[index].id = rename;
                            for (auto& child : replacement.elements) if (child.parent == selected) child.parent = rename;
                            static_cast<void>(replacement.Serialize()); FinishEdit(); document = std::move(replacement); selected = rename; Checkpoint(before);
                            error.clear(); ImGui::CloseCurrentPopup();
                        } catch (const std::exception& exception) { error = exception.what(); }
                    }
                    ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
            ImGui::PopItemWidth();
        }
    }
    ImGui::End();
}

} // namespace Concord::Editor
