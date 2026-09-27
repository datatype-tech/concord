// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/ui/UiView.h"
#include "engine/ui/PlayHost.h"
#include "Concord/CWindow.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <vector>

namespace Concord {
struct UiView::Impl {
    UiDocument document;
    bool open = false;
    bool overridePlace = false;
    bool overrideRect = false;
    UiPlace place = UiPlace::Fill;
    Vec2 position{};
    Vec2 size{};
    std::vector<UiEvent> events;
};

std::vector<UiView::Impl*>& UiView::Views()
{
    static std::vector<Impl*> views;
    return views;
}

UiView::UiView(std::filesystem::path path) : m_impl(std::make_unique<Impl>())
{
    m_impl->document.Load(std::move(path));
    Views().push_back(m_impl.get());
}

UiView::~UiView()
{
    if (m_impl) std::erase(Views(), m_impl.get());
}

UiView::UiView(UiView&&) noexcept = default;
UiView& UiView::operator=(UiView&& other) noexcept
{
    if (this == &other) return *this;
    if (m_impl) std::erase(Views(), m_impl.get());
    m_impl = std::move(other.m_impl);
    return *this;
}

void UiView::Show()
{
    if (m_impl) m_impl->open = true;
}

void UiView::Close()
{
    if (!m_impl) return;
    m_impl->open = false;
    m_impl->events.clear();
}

bool UiView::IsOpen() const noexcept
{
    return m_impl && m_impl->open;
}

void UiView::SetPlace(UiPlace place)
{
    if (!m_impl || place < UiPlace::Free || place > UiPlace::Center) return;
    m_impl->overridePlace = true;
    m_impl->overrideRect = false;
    m_impl->place = place;
}

void UiView::SetRect(Vec2 position, Vec2 size)
{
    if (!m_impl) return;
    m_impl->overridePlace = true;
    m_impl->overrideRect = true;
    m_impl->place = UiPlace::Free;
    m_impl->position = position;
    m_impl->size = size;
}

const std::vector<UiEvent>& UiView::Events() const
{
    static const std::vector<UiEvent> empty;
    return m_impl ? m_impl->events : empty;
}

void UiView::Present(Window& window)
{
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (!context || !context->WithinFrameScope) return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;
    Vec2 origin{viewport->Pos.x, viewport->Pos.y};
    Vec2 size{viewport->Size.x, viewport->Size.y};
    if (PlayHost::Active()) {
        const float bar = PlayHost::BarHeight();
        origin.y += bar;
        size.y = std::max(1.0f, size.y - bar);
    }
    const UiDrawDesc host{.position = origin, .size = size, .interactive = true, .window = &window};
    const std::vector<Impl*> views = Views();
    int slot = 0;
    for (Impl* view : views) {
        if (!view || !view->open) continue;
        UiDocument& document = view->document;
        const UiPlace filePlace = document.place;
        const Vec2 filePosition = document.placePosition;
        const Vec2 fileSize = document.placeSize;
        if (view->overridePlace) {
            document.place = view->place;
            if (view->overrideRect) {
                document.placePosition = view->position;
                document.placeSize = view->size;
            }
        }
        const UiDrawDesc region = document.Region(host);
        document.place = UiPlace::Fill;
        ImGui::PushID(slot++);
        ImGui::SetNextWindowPos({region.position.x, region.position.y});
        ImGui::SetNextWindowSize({std::max(1.0f, region.size.x), std::max(1.0f, region.size.y)});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("##ConcordUiView", nullptr, flags)) {
            try {
                view->events = document.Draw({.position = region.position, .size = region.size, .interactive = true, .window = &window});
            } catch (...) {
                view->events.clear();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopID();
        document.place = filePlace;
        document.placePosition = filePosition;
        document.placeSize = fileSize;
    }
}
}
