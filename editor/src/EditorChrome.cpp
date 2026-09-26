// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include <imgui_internal.h>
#include <algorithm>
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
        else ImGui::OpenPopup("Close workspace?");
    }
    if(ImGui::BeginPopupModal("Close workspace?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save your changes before closing?");
        if(m_process.Busy())ImGui::TextUnformatted("The editor's running build or game will also stop.");
        if(ImGui::Button("Save and close")) {
            if(m_uiDesigner.Dirty() && m_uiDesigner.Path().empty()) {
                ImGui::CloseCurrentPopup();SwitchPage(3);m_uiDesigner.Save();
                m_status="Save the new interface, then close the workspace again.";
            } else Attempt([&]{Save();m_window.RequestClose();ImGui::CloseCurrentPopup();});
        }
        ImGui::SameLine();if(ImGui::Button("Close without saving")) {m_window.RequestClose();ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
        ImGui::TextWrapped("%s",m_status.c_str());
        ImGui::EndPopup();
    }
}
}
