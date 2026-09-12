// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ui/UiCanvas.h"

#include <cstring>

namespace Concord {
namespace {

void WriteText(char* destination, const char* text) noexcept
{
    if (destination == nullptr) {
        return;
    }
    if (text == nullptr) {
        destination[0] = '\0';
        return;
    }
    std::strncpy(destination, text, kUiTextCapacity - 1);
    destination[kUiTextCapacity - 1] = '\0';
}

bool Contains(f32 x, f32 y, f32 width, f32 height, f32 px, f32 py) noexcept
{
    return px >= x && py >= y && px <= x + width && py <= y + height;
}

} // namespace

void UiCanvas::Begin(f32 mouseX, f32 mouseY, bool mousePressed, f32 width, f32 height) noexcept
{
    m_list.commands.clear();
    m_mouseX = mouseX;
    m_mouseY = mouseY;
    m_mousePressed = mousePressed;
    m_width = width;
    m_height = height;
    m_open = true;
}

void UiCanvas::End() noexcept
{
    m_open = false;
}

void UiCanvas::Panel(f32 x, f32 y, f32 width, f32 height) noexcept
{
    if (!m_open || width <= 0.0f || height <= 0.0f) {
        return;
    }
    UiDrawCommand command{};
    command.kind = UiDrawKind::Rect;
    command.x = x;
    command.y = y;
    command.width = width;
    command.height = height;
    command.color = theme.panel;
    m_list.commands.push_back(command);
}

void UiCanvas::Label(f32 x, f32 y, const char* text) noexcept
{
    if (!m_open || text == nullptr || text[0] == '\0') {
        return;
    }
    UiDrawCommand command{};
    command.kind = UiDrawKind::Text;
    command.x = x;
    command.y = y;
    command.height = theme.lineHeight;
    command.color = theme.text;
    WriteText(command.text, text);
    m_list.commands.push_back(command);
}

bool UiCanvas::Button(f32 x, f32 y, f32 width, f32 height, const char* text) noexcept
{
    if (!m_open || width <= 0.0f || height <= 0.0f) {
        return false;
    }
    const bool hot = Contains(x, y, width, height, m_mouseX, m_mouseY);
    UiDrawCommand rect{};
    rect.kind = UiDrawKind::Rect;
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
    rect.color = hot ? theme.buttonHot : theme.button;
    m_list.commands.push_back(rect);
    if (text != nullptr && text[0] != '\0') {
        UiDrawCommand label{};
        label.kind = UiDrawKind::Text;
        label.x = x + 8.0f;
        label.y = y + (height - theme.lineHeight) * 0.5f;
        label.height = theme.lineHeight;
        label.color = theme.text;
        WriteText(label.text, text);
        m_list.commands.push_back(label);
    }
    return hot && m_mousePressed;
}

} // namespace Concord
