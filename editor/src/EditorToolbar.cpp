// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include <imgui_internal.h>
#include <algorithm>

namespace Concord::Editor {
void Workspace::Toolbar()
{
    auto* viewport=ImGui::GetMainViewport();
    const float unit=ImGui::GetFontSize(),frame=ImGui::GetFrameHeight(),padding=unit*0.65f;
    const float titleHeight=std::max(frame,unit*1.35f)+unit*0.7f;
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{padding,unit*0.35f});
    if(ImGui::BeginViewportSideBar("Title",viewport,ImGuiDir_Up,titleHeight,flags)) {
        Design::Image("brand",frame,ImGui::GetColorU32(Design::Accent));
        if(viewport->Size.x>unit*33) {
            ImGui::SameLine(0,unit*0.5f);ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("CONCORD FLASH");
            if(viewport->Size.x>unit*65 || m_projectManager) {
                ImGui::SameLine(0,unit);const std::string project=Utf8Text(m_project.filename());
                ImGui::TextColored(Design::Muted,"%s",m_projectManager?"Project manager":project.substr(0,26).c_str());
            }
        }
        const float controlsStart=viewport->Size.x-padding-frame*4;
        float dragStart=ImGui::GetItemRectMax().x-viewport->Pos.x+padding;
        if(!m_projectManager) {
            const bool labels=viewport->Size.x>unit*35;
            const float tabsWidth=labels?3*unit+ImGui::CalcTextSize("3DCodeUI").x+9*ImGui::GetStyle().FramePadding.x+unit*0.5f:3*frame+unit*0.5f;
            ImGui::SameLine(std::max(dragStart,std::min(viewport->Size.x*0.42f,controlsStart-tabsWidth-padding)));
            if(Design::Action("Scene workspace","cube",labels?"3D":nullptr,m_page==1))SwitchPage(1);
            ImGui::SameLine(0,unit*0.25f);
            if(Design::Action("Code workspace","code",labels?"Code":nullptr,m_page==2))SwitchPage(2);
            ImGui::SameLine(0,unit*0.25f);
            if(Design::Action("UI workspace","grid",labels?"UI":nullptr,m_page==3))SwitchPage(3);
            dragStart=ImGui::GetItemRectMax().x-viewport->Pos.x+padding;
        }
        m_window.SetDragRegion({dragStart,0},{std::max(0.0f,controlsStart-dragStart),titleHeight});
        ImGui::SameLine(controlsStart);ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
        if(Design::Action("Fullscreen (F11)","fullscreen"))m_window.ToggleFullscreen();ImGui::SameLine();
        if(Design::Action("Minimize","minimize"))m_window.Minimize();ImGui::SameLine();
        if(Design::Action(m_window.IsMaximized()?"Restore":"Maximize","maximize")) {
            if(m_window.Mode()==WindowMode::Fullscreen)m_window.ToggleFullscreen();
            else if(m_window.IsMaximized())m_window.Restore();else m_window.Maximize();
        }
        ImGui::SameLine();if(Design::Action("Close window","close"))m_closeRequested=true;
        ImGui::PopStyleVar();
    }
    ImGui::End();ImGui::PopStyleVar();ImGui::PopStyleColor();
    if(!m_projectManager) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{padding,0});
        if(ImGui::BeginViewportSideBar("Commands",viewport,ImGuiDir_Up,frame,flags|ImGuiWindowFlags_MenuBar)) {
            if(ImGui::BeginMenuBar()) {
                if(ImGui::BeginMenu("Project")) {
                    if(ImGui::MenuItem("Project manager..."))Attempt([&]{LaunchWorkspace({},Utf8Path(m_cli),Utf8Path(m_sdk));});
                    if(ImGui::MenuItem("New scene..."))RequestSceneFile(false);
                    if(ImGui::MenuItem("Save scene as..."))RequestSceneFile(true);
                    if(ImGui::MenuItem("New UI document...")){m_uiDesigner.NewDocument();SwitchPage(3);}
                    if(ImGui::MenuItem("Project settings..."))m_showProjectSettings=true;
                    ImGui::Separator();
                    if(ImGui::MenuItem("Save all","Ctrl+S"))Attempt([&]{Save();});
                    if(ImGui::MenuItem("Preferences..."))m_showPreferences=true;
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Edit")) {
                    if(ImGui::MenuItem("Undo","Ctrl+Z")){if(m_page==2)m_code.Undo();else if(m_page==3)m_uiDesigner.Undo();else Undo();}
                    if(ImGui::MenuItem("Redo","Ctrl+Shift+Z")){if(m_page==2)m_code.Redo();else if(m_page==3)m_uiDesigner.Redo();else Undo(true);}
                    if(ImGui::MenuItem("Duplicate object","Ctrl+D",false,m_page==1&&IsSelected()))DuplicateSelection();
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("View")) {
                    if(ImGui::MenuItem("Restore default layout"))RestoreLayout();
                    ImGui::MenuItem("Build output",nullptr,&m_showOutput);
                    ImGui::MenuItem("Scene grid",nullptr,&m_showGrid);
                    if(ImGui::MenuItem("Preferences..."))m_showPreferences=true;
                    ImGui::EndMenu();
                }
                const bool labels=viewport->Size.x>unit*38;
                const float actionWidth=frame*4+unit*1.5f+(labels?ImGui::CalcTextSize("BuildPlay").x+4*ImGui::GetStyle().FramePadding.x:0);
                ImGui::SameLine(std::max(ImGui::GetCursorPosX(),viewport->Size.x-actionWidth-padding));
                if(Design::Action("Save all (Ctrl+S)","save"))Attempt([&]{Save();});ImGui::SameLine(0,unit*0.3f);
                ImGui::BeginDisabled(m_process.Busy());
                if(Design::Action("Build project","build",labels?"Build":nullptr))Attempt([&]{Build(false);});ImGui::SameLine(0,unit*0.3f);
                if(Design::Action("Play game (F5)","play",labels?"Play":nullptr,false,true))Attempt([&]{Build(true);});
                ImGui::EndDisabled();ImGui::SameLine(0,unit*0.3f);
                ImGui::BeginDisabled(!m_process.Busy());if(Design::Action("Stop process","stop"))m_process.Stop();ImGui::EndDisabled();
                ImGui::EndMenuBar();
            }
        }
        ImGui::End();ImGui::PopStyleVar();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{padding,unit*0.22f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    if(ImGui::BeginViewportSideBar("Status",viewport,ImGuiDir_Down,unit*1.5f,flags)) {
        if(!m_projectManager) {
            Design::Image("console",unit);ImGui::SameLine();
            if(ImGui::Selectable(m_showOutput?"Output -":"Output +",false,0,{unit*4.2f,unit}))m_showOutput=!m_showOutput;
            ImGui::SameLine();ImGui::TextColored(Design::Muted,"| %.0f fps | %s",ImGui::GetIO().Framerate,m_sceneDirty||m_scriptDirty||m_uiDesigner.Dirty()?"Unsaved":"Saved");ImGui::SameLine(0,unit);
        }
        ImGui::TextColored(Design::Muted,"%s",m_status.c_str());
    }
    ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar();
}
}
