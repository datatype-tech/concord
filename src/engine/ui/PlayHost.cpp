// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/ui/PlayHost.h"
#include "Concord/CWindow.h"

#include <imgui.h>
#include <imgui_internal.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdlib>

namespace Concord {
namespace {
bool& DismissedFlag()
{
    static bool dismissed = false;
    return dismissed;
}

bool ChineseSession()
{
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}
}

bool PlayHost::Active()
{
    if (DismissedFlag()) return false;
    const char* value = std::getenv("CONCORD_PLAY_HOST");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

float PlayHost::BarHeight()
{
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (!context || !context->WithinFrameScope) return 32.0f;
    return std::max(32.0f, ImGui::GetFrameHeight() + 8.0f);
}

void PlayHost::Dismiss()
{
    DismissedFlag() = true;
}

void PlayHost::Draw(Window& window)
{
    if (!Active()) return;
    const ImGuiContext* context = ImGui::GetCurrentContext();
    if (!context || !context->WithinFrameScope) return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;
    const float bar = BarHeight();
    const bool chinese = ChineseSession();
    const char* direct = chinese ? "直接窗口" : "Direct window";
    const char* stop = chinese ? "停止" : "Stop";
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize({viewport->Size.x, bar});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 4});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.147f, 0.168f, 0.203f, 1});
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.520f, 0.570f, 0.640f, 0.45f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.275f, 0.310f, 0.365f, 1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.330f, 0.368f, 0.428f, 1});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.275f, 0.455f, 0.655f, 1});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.925f, 0.940f, 0.960f, 1});
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##ConcordPlayHost", nullptr, flags)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("CONCORD");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", window.Title().c_str());
        const float stopWidth = ImGui::CalcTextSize(stop).x + ImGui::GetStyle().FramePadding.x * 2.0f + 18.0f;
        const float directWidth = ImGui::CalcTextSize(direct).x + ImGui::GetStyle().FramePadding.x * 2.0f + 18.0f;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - stopWidth - directWidth - 12.0f));
        if (ImGui::Button(direct, {directWidth, 0})) Dismiss();
        ImGui::SameLine();
        if (ImGui::Button(stop, {stopWidth, 0})) window.RequestClose();
        const ImVec2 origin = ImGui::GetWindowPos();
        const float width = ImGui::GetWindowSize().x;
        ImGui::GetWindowDrawList()->AddLine({origin.x, origin.y + bar - 1.0f}, {origin.x + width, origin.y + bar - 1.0f},
            IM_COL32(0, 0, 0, 90));
    }
    ImGui::End();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(5);
}
}
