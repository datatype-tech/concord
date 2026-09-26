// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/UiDesignerState.h"
#include "editor/Design.h"
#include "editor/NativeDialogs.h"
#include "editor/Localization.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

namespace Concord::Editor {
namespace {
const char* KindName(UiElementKind kind)
{
    static const char* names[]{"Panel", "Label", "Button", "Checkbox", "Slider", "Text input", "Progress"};
    return Tr(names[static_cast<int>(kind)]);
}
bool Inside(ImVec2 point, Vec2 position, Vec2 size)
{
    return point.x >= position.x && point.y >= position.y && point.x < position.x + size.x && point.y < position.y + size.y;
}
void InlineIfFits(float width)
{
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right) ImGui::SameLine();
}
/** Matches runtime parent-before-child painting even when file order differs. */
std::vector<size_t> PaintOrder(const UiDocument& document)
{
    std::unordered_map<std::string, std::vector<size_t>> children;
    for (size_t index = 0; index < document.elements.size(); ++index) children[document.elements[index].parent].push_back(index);
    std::vector<size_t> order;
    std::function<void(const std::string&)> visit = [&](const std::string& parent) {
        const auto found = children.find(parent);
        if (found == children.end()) return;
        for (size_t child : found->second) { order.push_back(child); visit(document.elements[child].id); }
    };
    visit(""); return order;
}
}

void UiDesigner::Impl::Hierarchy()
{
    if (ImGui::Begin(TrId("UI Elements").c_str())) {
        ImGui::BeginDisabled(!loaded);
        if (Design::Action("##addUiElement", "plus", Tr("Add"))) ImGui::OpenPopup("##addUiElementMenu");
        if (ImGui::BeginPopup("##addUiElementMenu")) {
            for (int kind = 0; kind < 7; ++kind)
                if (ImGui::MenuItem(KindName(static_cast<UiElementKind>(kind)))) Add(static_cast<UiElementKind>(kind));
            ImGui::EndPopup();
        }
        InlineIfFits(ImGui::GetFrameHeight()); ImGui::BeginDisabled(SelectedIndex() < 0);
        if (Design::Action("Duplicate UI element", "copy")) Duplicate();
        InlineIfFits(ImGui::GetFrameHeight()); if (Design::Action("Delete element and children", "delete")) Delete();
        ImGui::EndDisabled(); ImGui::EndDisabled();
        ImGui::Separator();
        if (loaded) {
            if (ImGui::Selectable(Tr("Document canvas"), selected.empty())) Select("");
            bool remove = false, duplicate = false;
            std::function<void(const std::string&, int)> visit = [&](const std::string& parent, int depth) {
                if (depth > 64) return;
                for (const auto& element : document.elements) {
                    if (element.parent != parent) continue;
                    const bool children = std::any_of(document.elements.begin(), document.elements.end(), [&](const auto& other) { return other.parent == element.id; });
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                    if (!children) flags |= ImGuiTreeNodeFlags_Leaf;
                    if (selected == element.id) flags |= ImGuiTreeNodeFlags_Selected;
                    if (!element.visible) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    const bool expanded = ImGui::TreeNodeEx(element.id.c_str(), flags, "%s", element.id.c_str());
                    if (!element.visible) ImGui::PopStyleColor();
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) Select(element.id);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s", KindName(element.kind), element.visible ? "" : Tr(" (hidden)"));
                    if (ImGui::BeginPopupContextItem()) {
                        Select(element.id);
                        if (ImGui::MenuItem(Tr("Duplicate subtree"))) duplicate = true;
                        if (ImGui::MenuItem(Tr("Delete subtree"))) remove = true;
                        ImGui::EndPopup();
                    }
                    if (expanded) { if (children) visit(element.id, depth + 1); ImGui::TreePop(); }
                }
            };
            visit("", 0);
            if (remove) Delete(); else if (duplicate) Duplicate();
            ImGui::Separator(); ImGui::TextDisabled(Tr("%zu elements"), document.elements.size());
            ImGui::TextWrapped("%s", Tr("Select a panel before adding a child. Parent and anchors are editable in Properties."));
        } else ImGui::TextWrapped("%s", Tr("Create or open a .yu file to design an interface."));
    }
    ImGui::End();
}

void UiDesigner::Impl::Canvas()
{
    const auto flags = loaded && Dirty() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;
    if (ImGui::Begin(TrId("UI Canvas").c_str(), nullptr, flags)) {
        if (Design::Action("New UI document", "plus")) Request(Pending::New);
        InlineIfFits(ImGui::GetFrameHeight()); if (Design::Action("Open .yu document", "folder")) {
            error.clear();
            const auto folder = std::filesystem::is_directory(project / "UI") ? project / "UI" : project;
            const auto file = project.empty() ? std::filesystem::path{} : ChooseFile(WideText(Tr("Open UI document")).c_str(), L"Concord UI (*.yu)", L"*.yu", folder);
            if (!file.empty()) Request(Pending::Load, file);
        }
        InlineIfFits(ImGui::GetFrameHeight()); ImGui::BeginDisabled(!loaded);
        if (Design::Action("Save UI document", "save")) Save();
        InlineIfFits(ImGui::GetFrameHeight()); ImGui::BeginDisabled(undo.empty()); if (Design::Action("Undo UI edit", "undo")) Undo(false); ImGui::EndDisabled();
        InlineIfFits(ImGui::GetFrameHeight()); ImGui::BeginDisabled(redo.empty()); if (Design::Action("Redo UI edit", "redo")) Undo(true); ImGui::EndDisabled();
        InlineIfFits(ImGui::GetFrameHeight()); if (Design::Action("Fit UI canvas", "focus", nullptr, fit)) fit = !fit;
        InlineIfFits(ImGui::GetFrameHeight()); if (Design::Action("Show UI grid", "grid", nullptr, showGrid)) showGrid = !showGrid;
        InlineIfFits(ImGui::GetFrameHeight() + ImGui::CalcTextSize(Tr("Snap")).x + ImGui::GetStyle().ItemInnerSpacing.x); ImGui::Checkbox(TrId("Snap").c_str(), &snap);
        ImGui::EndDisabled();
        if (!loaded) {
            ImGui::Spacing(); Design::Heading(Tr("Design your interface"));
            ImGui::TextWrapped("%s", Tr("Create reusable panels, controls and window buttons. The canvas uses Concord's runtime UI renderer."));
            if (Design::Action("##createInterface", "plus", Tr("Create interface"), false, true)) Request(Pending::New);
        } else {
            ImGui::TextDisabled("%s%s", path.empty() ? "Untitled.yu" : Utf8Text(path.filename()).c_str(), Dirty() ? " *" : "");
            InlineIfFits(ImGui::GetFontSize() * 9); ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
            if (ImGui::SliderFloat(TrId("Zoom").c_str(), &zoom, 0.1f, 2.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp)) fit = false;
            const float footer = ImGui::GetTextLineHeightWithSpacing() + (error.empty() ? 0 :
                ImGui::CalcTextSize(error.c_str(), nullptr, false, std::max(1.0f, ImGui::GetContentRegionAvail().x)).y + ImGui::GetStyle().ItemSpacing.y);
            ImGui::BeginChild("Canvas surface", {0, -footer}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float margin = 24.0f;
            const float fitted = std::max(0.001f, std::min(std::max(1.0f, available.x - margin * 2) / document.referenceSize.x,
                                                       std::max(1.0f, available.y - margin * 2) / document.referenceSize.y));
            const float scale = fit ? fitted : zoom;
            const ImVec2 size{document.referenceSize.x * scale, document.referenceSize.y * scale};
            auto origin = ImGui::GetCursorScreenPos();
            origin.x += std::max(margin, (available.x - size.x) * 0.5f);
            origin.y += std::max(margin, (available.y - size.y) * 0.5f);
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton("##uiSurface", size, ImGuiButtonFlags_MouseButtonLeft);
            const bool hovered = ImGui::IsItemHovered();
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled({origin.x + 5, origin.y + 7}, {origin.x + size.x + 5, origin.y + size.y + 7}, IM_COL32(0,0,0,50), 2);
            draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(20,23,30,255));
            draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
            if (showGrid) {
                float spacing = grid * scale;
                while (spacing < 14.0f) spacing *= 2;
                const auto clipMin = draw->GetClipRectMin(), clipMax = draw->GetClipRectMax();
                const float firstX = origin.x + std::max(0.0f, std::floor((clipMin.x - origin.x) / spacing)) * spacing;
                const float firstY = origin.y + std::max(0.0f, std::floor((clipMin.y - origin.y) / spacing)) * spacing;
                for (float x = firstX; x < std::min(origin.x + size.x, clipMax.x); x += spacing)
                    draw->AddLine({x, origin.y}, {x, origin.y + size.y}, IM_COL32(78,89,111,35));
                for (float y = firstY; y < std::min(origin.y + size.y, clipMax.y); y += spacing)
                    draw->AddLine({origin.x, y}, {origin.x + size.x, y}, IM_COL32(78,89,111,35));
            }
            UiDrawDesc desc{.position = {origin.x, origin.y}, .size = {size.x, size.y}, .interactive = false};
            const auto layouts = document.ResolveLayout(desc);
            const auto events = document.Draw(desc);
            static_cast<void>(events);
            const auto mouse = ImGui::GetMousePos();
            int index = SelectedIndex();
            bool handle = false;
            if (index >= 0) {
                const auto& rect = layouts[index];
                const ImVec2 min{rect.position.x, rect.position.y}, max{rect.position.x + rect.size.x, rect.position.y + rect.size.y};
                const auto accent = ImGui::GetColorU32(Design::Accent);
                draw->AddRect(min, max, accent, 0, 0, 1.5f);
                draw->AddRectFilled({max.x - 5, max.y - 5}, {max.x + 5, max.y + 5}, accent, 1);
                handle = std::abs(mouse.x - max.x) <= 8 && std::abs(mouse.y - max.y) <= 8 && hovered;
                if (handle) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            }
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                FinishEdit();
                if (!handle) {
                    std::string hit;
                    const auto order = PaintOrder(document);
                    for (auto it = order.rbegin(); it != order.rend(); ++it) {
                        const auto& layout = layouts[*it];
                        if (layout.visible && Inside(mouse, layout.position, layout.size) && Inside(mouse, layout.clipPosition, layout.clipSize)) { hit = document.elements[*it].id; break; }
                    }
                    Select(hit); index = SelectedIndex();
                }
                if (index >= 0) {
                    dragging = true; resizing = handle; dragMouse = mouse;
                    dragElement = document.elements[index]; dragBefore = document.Serialize();
                }
            }
            if (dragging) {
                index = SelectedIndex();
                if (index >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    auto& element = document.elements[index];
                    Vec2 delta{(mouse.x - dragMouse.x) / scale, (mouse.y - dragMouse.y) / scale};
                    const bool snapping = snap != ImGui::GetIO().KeyShift;
                    if (resizing) {
                        Vec2 dimensions{dragElement.size.x + delta.x, dragElement.size.y + delta.y};
                        if (snapping) { dimensions.x = std::round(dimensions.x / grid) * grid; dimensions.y = std::round(dimensions.y / grid) * grid; }
                        element.size = {std::clamp(dimensions.x, 1.0f, 100000.0f), std::clamp(dimensions.y, 1.0f, 100000.0f)};
                        element.position = {dragElement.position.x + dragElement.anchor.x * (element.size.x - dragElement.size.x),
                                            dragElement.position.y + dragElement.anchor.y * (element.size.y - dragElement.size.y)};
                    } else {
                        element.position = {dragElement.position.x + delta.x, dragElement.position.y + delta.y};
                        if (snapping) { element.position.x = std::round(element.position.x / grid) * grid; element.position.y = std::round(element.position.y / grid) * grid; }
                    }
                    element.position.x = std::clamp(element.position.x, -1000000.0f, 1000000.0f);
                    element.position.y = std::clamp(element.position.y, -1000000.0f, 1000000.0f);
                }
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) { dragging = false; Checkpoint(dragBefore); dragBefore.clear(); }
            }
            if (hovered && !ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_Delete)) Delete();
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) Duplicate();
            }
            draw->PopClipRect();
            draw->AddRect(origin, {origin.x + size.x, origin.y + size.y}, ImGui::GetColorU32(ImGuiCol_Border));
            ImGui::EndChild();
            ImGui::TextDisabled(Tr("%.0f%%  |  Drag to move, corner to resize  |  Shift toggles snapping"), scale * 100);
        }
        if (!error.empty()) { ImGui::PushTextWrapPos(); ImGui::TextColored({1,0.55f,0.45f,1}, "%s", error.c_str()); ImGui::PopTextWrapPos(); }
    }
    ImGui::End();
}

} // namespace Concord::Editor
