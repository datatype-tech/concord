// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Concord::Editor {
void Workspace::SwitchPage(int page)
{
    if(page>=1 && page<=3)m_pendingPage=page;
}
void Workspace::OpenLayouts()
{
    if(m_projectManager || m_project.empty())return;
    m_workspaceLayouts.Open(m_project/".editor"/"layouts");
    m_page=m_workspaceLayouts.LastPage();m_pendingPage=0;m_resetLayout=false;
    m_layoutPath.clear();
}
void Workspace::SaveLayouts()
{
    if(!m_projectManager && !m_project.empty())m_workspaceLayouts.Save();
}
void Workspace::RestoreLayout()
{
    m_resetLayout=true;
}
void Workspace::DockLayout()
{
    auto& io=ImGui::GetIO();
    if(io.KeyCtrl && !io.WantTextInput) {
        if(ImGui::IsKeyPressed(ImGuiKey_1,false))SwitchPage(1);
        if(ImGui::IsKeyPressed(ImGuiKey_2,false))SwitchPage(2);
        if(ImGui::IsKeyPressed(ImGuiKey_3,false))SwitchPage(3);
        if(ImGui::IsKeyPressed(ImGuiKey_Comma,false))m_showPreferences=true;
    }
    if(m_pendingPage) {m_page=m_pendingPage;m_pendingPage=0;}
    m_compact=ImGui::GetMainViewport()->WorkSize.x<ImGui::GetFontSize()*58;
    const bool revealOutput=m_showOutput && (m_process.Busy() || m_wasBusy);
    m_workspaceLayouts.Draw(m_page,m_compact,m_showOutput,m_resetLayout,revealOutput);
    m_resetLayout=false;
}
void Workspace::CloseDialog()
{
    if(m_closeRequested) {
        m_closeRequested=false;
        if(!m_sceneDirty && !m_scriptDirty && !m_uiDesigner.Dirty() && !m_process.Busy())m_window.RequestClose();
        else ImGui::OpenPopup("###CloseWorkspace");
    }
    if(Design::BeginDialog(TrId("Close workspace","CloseWorkspace").c_str(),30)) {
        Design::Heading(Tr("Save your changes before closing?"));
        ImGui::Spacing();
        ImGui::TextWrapped("%s",Tr("Unsaved edits to scripts, the scene or the interface will be lost if you close without saving."));
        if(m_process.Busy())ImGui::TextColored({0.96f,0.70f,0.40f,1},"%s",Tr("The running build or game will also stop."));
        ImGui::Spacing();ImGui::Spacing();
        const float width=Design::ButtonWidth(Tr("Save and close"),false)+Design::ButtonWidth(Tr("Don't save"),false)+
            Design::ButtonWidth(Tr("Cancel"),false)+ImGui::GetStyle().ItemSpacing.x*2;
        Design::AlignRight(width);
        if(Design::Action("##saveClose","",Tr("Save and close"),false,true)) {
            if(m_uiDesigner.Dirty() && m_uiDesigner.Path().empty()) {
                ImGui::CloseCurrentPopup();SwitchPage(3);m_uiDesigner.Save();
                m_status=Tr("Save the new interface, then close the workspace again.");
            } else Attempt([&]{Save();m_window.RequestClose();ImGui::CloseCurrentPopup();});
        }
        ImGui::SameLine();if(Design::Action("##discardClose","",Tr("Don't save"))) {m_window.RequestClose();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(Design::Action("##cancelClose","",Tr("Cancel")))ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
void Workspace::AboutDialog()
{
    if(m_showAbout){ImGui::OpenPopup("###About");m_showAbout=false;}
    if(!Design::BeginDialog(TrId("About Concord Flash","About").c_str(),28))return;
    const float unit=ImGui::GetFontSize(),logo=std::round(unit*2.4f);
    const ImVec2 brand=ImGui::GetCursorScreenPos();
    Design::Logo(brand,logo);
    ImGui::Dummy({logo,logo});
    ImGui::SameLine(0,unit*0.55f);
    ImGui::BeginGroup();
    auto& fonts=ImGui::GetIO().Fonts->Fonts;
    if(fonts.Size>2)ImGui::PushFont(fonts[2]);
    ImGui::TextUnformatted("Concord Flash");
    if(fonts.Size>2)ImGui::PopFont();
    ImGui::TextColored(Design::Muted,"%s %s",Tr("Version"),EditorVersion);
    ImGui::EndGroup();
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
    if(ImGui::BeginTable("stack",2,ImGuiTableFlags_SizingStretchProp)) {
        const std::pair<const char*,const char*> rows[]={
            {"Renderer","Vulkan 1.4  /  Forward+"},{"Physics","Jolt Physics 5.6"},{"Audio","Steam Audio 4.8"},
            {"Platform","SDL3  /  Windows 10, 11"},{"Interface","Dear ImGui  /  ImGuizmo"},{"License","Mozilla Public License 2.0"}};
        for(const auto& [label,value]:rows) {
            ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextColored(Design::Muted,"%s",Tr(label));
            ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(value);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    const float width=Design::ButtonWidth("GitHub",false)+Design::ButtonWidth(Tr("Close"),false)+ImGui::GetStyle().ItemSpacing.x;
    Design::AlignRight(width);
    if(Design::Action("##github","","GitHub"))OpenExternal(L"https://github.com/datatype-tech/concord");
    ImGui::SameLine();if(Design::Action("##closeAbout","",Tr("Close"),false,true))ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Workspace::ShortcutsDialog()
{
    if(m_showShortcuts){ImGui::OpenPopup("###Shortcuts");m_showShortcuts=false;}
    if(!Design::BeginDialog(TrId("Keyboard shortcuts","Shortcuts").c_str(),34))return;
    struct Shortcut {const char* keys;const char* action;};
    const auto group=[](const char* title,std::initializer_list<Shortcut> shortcuts) {
        ImGui::SeparatorText(Tr(title));
        if(ImGui::BeginTable(title,2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("keys",ImGuiTableColumnFlags_WidthFixed,ImGui::GetFontSize()*9);
            for(const auto& shortcut:shortcuts) {
                ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::TextColored(Design::Accent,"%s",shortcut.keys);
                ImGui::TableSetColumnIndex(1);ImGui::TextUnformatted(Tr(shortcut.action));
            }
            ImGui::EndTable();
        }
    };
    group("General",{{"Ctrl+S","Save all"},{"F6","Preview open scene"},{"F5","Build and play"},{"Shift+F5","Stop"},{"Ctrl+B","Build project"},
        {"Ctrl+O","Open scene..."},{"Ctrl+1 / 2 / 3","Switch workspace"},{"Ctrl+,","Preferences..."},{"F11","Full screen"},{"F1","Keyboard shortcuts"}});
    group("3D scene",{{"W / E / R","Move / rotate / scale"},{"RMB + drag","Orbit the view"},{"RMB + W A S D Q E","Fly through the scene"},
        {"MMB + drag","Pan the view"},{"Wheel","Zoom"},{"F / Home","Focus selection / frame all"},{"Ctrl+C / V / D","Copy / paste / duplicate"},
        {"F2 / Del","Rename / delete"},{"Ctrl+Z / Ctrl+Y","Undo / redo"}});
    group("Code",{{"Ctrl+F / F3","Find / find next"},{"Ctrl+G","Go to line"},{"Ctrl+Space","Complete word"}});
    ImGui::Spacing();
    Design::AlignRight(Design::ButtonWidth(Tr("Close"),false));
    if(Design::Action("##closeShortcuts","",Tr("Close"),false,true))ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Workspace::ScreenTransition()
{
    const float elapsed=static_cast<float>(ImGui::GetTime()-m_transitionStart);
    constexpr float duration=0.32f;
    if(elapsed<0 || elapsed>=duration)return;
    const float t=elapsed/duration,eased=1.0f-std::pow(1.0f-t,3.0f);
    const auto* viewport=ImGui::GetMainViewport();
    auto background=ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);background.w=1.0f-eased;
    ImGui::GetForegroundDrawList()->AddRectFilled(viewport->Pos,{viewport->Pos.x+viewport->Size.x,viewport->Pos.y+viewport->Size.y},ImGui::GetColorU32(background));
}
}
