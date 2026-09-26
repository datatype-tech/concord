// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CUiDocument.h"
#include "Concord/CWindow.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace Concord {
namespace {
ImVec2 Point(Vec2 point) { return {point.x, point.y}; }

ImVec4 Color(ColorRGBA color, bool enabled)
{
    return {ColorR(color) / 255.0f, ColorG(color) / 255.0f, ColorB(color) / 255.0f,
        ColorA(color) / 255.0f * (enabled ? 1.0f : 0.45f)};
}

void Label(ImDrawList& draw, const UiElement& element, const UiElementLayout& layout,
           const std::string& text, bool centered)
{
    const float fontSize = ImGui::GetFontSize();
    ImVec2 position = Point(layout.position);
    if (centered) {
        const ImVec2 extent = ImGui::CalcTextSize(text.data(), text.data() + text.size(), false, layout.size.x);
        position.x += std::max(0.0f, (layout.size.x - extent.x) * 0.5f);
        position.y += std::max(0.0f, (layout.size.y - extent.y) * 0.5f);
    }
    draw.AddText(ImGui::GetFont(), fontSize, position, ImGui::ColorConvertFloat4ToU32(Color(element.color, layout.enabled)),
        text.data(), text.data() + text.size(), layout.size.x);
}

void WindowAction(Window& window, UiAction action)
{
    switch (action) {
    case UiAction::MinimizeWindow: window.Minimize(); break;
    case UiAction::ToggleMaximizeWindow:
        if (window.IsMaximized()) window.Restore(); else window.Maximize();
        break;
    case UiAction::ToggleFullscreenWindow: window.ToggleFullscreen(); break;
    case UiAction::CloseWindow: window.RequestClose(); break;
    case UiAction::None: break;
    }
}
}

std::vector<UiEvent> UiDocument::Draw(const UiDrawDesc& desc)
{
    auto* context = ImGui::GetCurrentContext();
    if (!context || !context->WithinFrameScope || !context->CurrentWindow)
        throw std::runtime_error("UiDocument::Draw requires an active ImGui frame and window");
    const auto layouts = ResolveLayout(desc);
    const float scale = std::min(desc.size.x / referenceSize.x, desc.size.y / referenceSize.y);
    std::unordered_map<std::string, usize> indices;
    for (usize index = 0; index < elements.size(); ++index) indices.emplace(elements[index].id, index);
    std::vector<std::vector<usize>> children(elements.size() + 1);
    for (usize index = 0; index < elements.size(); ++index)
        children[elements[index].parent.empty() ? elements.size() : indices.at(elements[index].parent)].push_back(index);
    std::vector<usize> order;
    std::function<void(usize)> visit = [&](usize parent) {
        for (usize child : children[parent]) { order.push_back(child); visit(child); }
    };
    visit(elements.size());
    std::vector<UiEvent> events;
    const ImVec2 savedCursor = ImGui::GetCursorScreenPos();
    const float savedFontScale = ImGui::GetCurrentWindow()->FontWindowScale;
    auto& draw = *ImGui::GetWindowDrawList();
    ImGui::PushID(this);
    for (usize index : order) {
        auto& element = elements[index];
        const auto& layout = layouts[index];
        if (!layout.visible) continue;
        ImGui::PushID(element.id.c_str());
        ImGui::PushClipRect(Point(layout.clipPosition), Point(layout.clipPosition + layout.clipSize), true);
        ImGui::SetWindowFontScale(savedFontScale * scale * element.fontScale);
        const float rounding = std::min({element.rounding * scale, layout.size.x * 0.5f, layout.size.y * 0.5f});
        const ImVec2 start = Point(layout.position), end = Point(layout.position + layout.size);
        ImVec4 background = Color(element.background, layout.enabled);
        const ImU32 foreground = ImGui::ColorConvertFloat4ToU32(Color(element.color, layout.enabled));
        bool clicked = false, changed = false, hovered = false;
        const bool control = element.kind == UiElementKind::Button || element.kind == UiElementKind::Checkbox ||
                             element.kind == UiElementKind::Slider || element.kind == UiElementKind::TextInput;
        if (desc.interactive && control) {
            ImGui::SetCursorScreenPos(start);
            ImGui::BeginDisabled(!layout.enabled);
            if (element.kind == UiElementKind::Button || element.kind == UiElementKind::Checkbox) {
                clicked = ImGui::InvisibleButton("control", Point(layout.size), ImGuiButtonFlags_EnableNav);
                hovered = ImGui::IsItemHovered();
                if (element.kind == UiElementKind::Checkbox && clicked) {
                    element.value = element.value >= 0.5f ? 0.0f : 1.0f;
                    changed = true;
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, Color(element.color, true));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, Color(element.background, true));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{background.x + 0.07f, background.y + 0.07f, background.z + 0.07f, background.w});
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{background.x + 0.12f, background.y + 0.12f, background.z + 0.12f, background.w});
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{8 * scale, std::max(0.0f, (layout.size.y - ImGui::GetFontSize()) * 0.5f)});
                ImGui::SetNextItemWidth(layout.size.x);
                if (element.kind == UiElementKind::Slider) {
                    changed = ImGui::SliderFloat("##control", &element.value, 0, 1, "", ImGuiSliderFlags_AlwaysClamp);
                } else {
                    std::vector<char> buffer(4097);
                    std::copy(element.input.begin(), element.input.end(), buffer.begin());
                    changed = ImGui::InputTextWithHint("##control", element.text.c_str(), buffer.data(), buffer.size());
                    if (changed) element.input = buffer.data();
                }
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(4);
            }
            ImGui::EndDisabled();
        }
        if (hovered) {
            background.x = std::min(1.0f, background.x + 0.07f);
            background.y = std::min(1.0f, background.y + 0.07f);
            background.z = std::min(1.0f, background.z + 0.07f);
        }
        const ImU32 surface = ImGui::ColorConvertFloat4ToU32(background);
        switch (element.kind) {
        case UiElementKind::Panel:
            draw.AddRectFilled(start, end, surface, rounding);
            Label(draw, element, layout, element.text, false);
            break;
        case UiElementKind::Label:
            Label(draw, element, layout, element.text, false);
            break;
        case UiElementKind::Button:
            draw.AddRectFilled(start, end, surface, rounding);
            Label(draw, element, layout, element.text, true);
            break;
        case UiElementKind::Checkbox: {
            const float box = std::min(layout.size.y, ImGui::GetFontSize() + 8 * scale);
            const ImVec2 checkStart{start.x, start.y + (layout.size.y - box) * 0.5f};
            draw.AddRectFilled(checkStart, {checkStart.x + box, checkStart.y + box}, surface, rounding);
            if (element.value >= 0.5f) {
                draw.AddLine({checkStart.x + box * 0.22f, checkStart.y + box * 0.5f},
                    {checkStart.x + box * 0.43f, checkStart.y + box * 0.72f}, foreground, std::max(1.0f, 2 * scale));
                draw.AddLine({checkStart.x + box * 0.43f, checkStart.y + box * 0.72f},
                    {checkStart.x + box * 0.8f, checkStart.y + box * 0.25f}, foreground, std::max(1.0f, 2 * scale));
            }
            auto label = layout;
            label.position.x += box + 8 * scale;
            label.size.x = std::max(1.0f, layout.size.x - box - 8 * scale);
            Label(draw, element, label, element.text, true);
            break;
        }
        case UiElementKind::Slider:
        case UiElementKind::Progress: {
            if (!desc.interactive || element.kind == UiElementKind::Progress) {
                draw.AddRectFilled(start, end, surface, rounding);
                const ImVec4 fill{std::min(1.0f, background.x + 0.2f), std::min(1.0f, background.y + 0.2f),
                    std::min(1.0f, background.z + 0.2f), background.w};
                if (element.value > 0) draw.AddRectFilled(start,
                    {start.x + layout.size.x * element.value, end.y}, ImGui::ColorConvertFloat4ToU32(fill), rounding);
            }
            char percent[24];
            std::snprintf(percent, sizeof(percent), " %.0f%%", element.value * 100);
            Label(draw, element, layout, element.text + percent, true);
            break;
        }
        case UiElementKind::TextInput:
            if (!desc.interactive) {
                draw.AddRectFilled(start, end, surface, rounding);
                auto label = layout;
                label.position.x += 8 * scale;
                label.size.x = std::max(1.0f, label.size.x - 16 * scale);
                Label(draw, element, label, element.input.empty() ? element.text : element.input, true);
            }
            break;
        }
        if (changed) events.push_back({element.id, UiEventKind::Changed, element.value, element.input, UiAction::None});
        else if (clicked) events.push_back({element.id, UiEventKind::Clicked, element.value, element.input, element.action});
        ImGui::PopClipRect();
        ImGui::PopID();
    }
    ImGui::SetWindowFontScale(savedFontScale);
    ImGui::PopID();
    if (desc.interactive) ImGui::SetCursorScreenPos(savedCursor);
    if (desc.window) for (const auto& event : events)
        if (event.kind == UiEventKind::Clicked) WindowAction(*desc.window, event.action);
    return events;
}
}
