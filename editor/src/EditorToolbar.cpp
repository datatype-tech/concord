// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace Concord::Editor {
namespace {
constexpr ImGuiWindowFlags BarFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|
    ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoFocusOnAppearing;
void VerticalDivider(float height)
{
    const auto position=ImGui::GetCursorScreenPos();const float unit=ImGui::GetFontSize();
    ImGui::GetWindowDrawList()->AddLine({position.x+unit*0.3f,position.y+height*0.22f},{position.x+unit*0.3f,position.y+height*0.78f},
        IM_COL32(255,255,255,32),1.0f);
    ImGui::Dummy({unit*0.6f,height});
}
}
void Workspace::Toolbar()
{
    TitleBar();
    if(!m_projectManager)CommandBar();
    StatusBar();
}
void Workspace::TitleBar()
{
    auto* viewport=ImGui::GetMainViewport();
    const float unit=ImGui::GetFontSize(),height=std::round(unit*1.9f);
    const ImVec4 bar=ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);
    const bool dimmed=ImGui::GetIO().AppFocusLost;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,bar);ImGui::PushStyleColor(ImGuiCol_MenuBarBg,bar);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{ImGui::GetStyle().FramePadding.x,(height-unit)*0.5f});
    const bool open=ImGui::BeginViewportSideBar("##TitleBar",viewport,ImGuiDir_Up,height,BarFlags|ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleVar(4);
    if(open && ImGui::BeginMenuBar()) {
        const ImVec2 origin=ImGui::GetWindowPos();const float width=ImGui::GetWindowWidth();
        auto* draw=ImGui::GetWindowDrawList();
        m_window.SetDragRegion({origin.x-viewport->Pos.x,origin.y-viewport->Pos.y},{width,height});
        const float logo=std::round(unit*1.45f),inset=std::round(unit*0.55f);
        Design::Logo({origin.x+inset,origin.y+std::round((height-logo)*0.5f)},logo);
        float menusEnd=origin.x+inset+logo;
        const float caption=std::round(height*1.42f);
        const float captionStart=origin.x+width-caption*4;
        if(!m_projectManager) {
            const float menusStart=origin.x+inset+logo+unit*0.55f;
            ImGui::SetCursorScreenPos({menusStart,origin.y+std::round((height-ImGui::GetFrameHeight())*0.5f)});
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text,dimmed?ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled):ImVec4{0.84f,0.86f,0.89f,1.0f});
            MainMenus();
            ImGui::PopStyleColor();
            menusEnd=ImGui::GetCursorScreenPos().x;
            m_window.ExcludeFromDragRegion({menusStart-viewport->Pos.x,0},{std::max(0.0f,menusEnd-menusStart),height});
        }
        std::string title=m_projectManager?std::string("Concord Flash  /  ")+Tr("Project Manager"):Utf8Text(m_project.filename())+"  -  Concord Flash";
        if(!m_projectManager && (m_sceneDirty || m_scriptDirty || m_uiDesigner.Dirty()))title="* "+title;
        const float titleWidth=ImGui::CalcTextSize(title.c_str()).x;
        const float titleX=std::clamp(origin.x+(width-titleWidth)*0.5f,menusEnd+unit,captionStart-unit-titleWidth);
        if(titleX>=menusEnd+unit*0.5f)
            draw->AddText({titleX,origin.y+(height-unit)*0.5f},ImGui::GetColorU32(dimmed?ImGuiCol_TextDisabled:ImGuiCol_Text,dimmed?1.0f:0.72f),title.c_str());
        const bool fullscreen=m_window.Mode()==WindowMode::Fullscreen,maximized=m_window.IsMaximized();
        m_window.ExcludeFromDragRegion({captionStart-viewport->Pos.x,0},{caption*4,height});
        ImGui::SetCursorScreenPos({captionStart,origin.y});
        if(Design::CaptionButton("##fullscreen",fullscreen?CaptionGlyph::ExitFullscreen:CaptionGlyph::Fullscreen,{caption,height},dimmed))ToggleFullscreen();
        Design::Tooltip(fullscreen?Tr("Exit full screen (F11)"):Tr("Full screen (F11)"));
        ImGui::SetCursorScreenPos({captionStart+caption,origin.y});
        if(Design::CaptionButton("##minimize",CaptionGlyph::Minimize,{caption,height},dimmed))m_window.Minimize();
        Design::Tooltip(Tr("Minimize"));
        ImGui::SetCursorScreenPos({captionStart+caption*2,origin.y});
        if(Design::CaptionButton("##maximize",maximized?CaptionGlyph::Restore:CaptionGlyph::Maximize,{caption,height},dimmed)) {
            if(fullscreen)ToggleFullscreen();else if(maximized)m_window.Restore();else m_window.Maximize();
        }
        Design::Tooltip(maximized?Tr("Restore down"):Tr("Maximize"));
        ImGui::SetCursorScreenPos({captionStart+caption*3,origin.y});
        if(Design::CaptionButton("##close",CaptionGlyph::Close,{caption,height},dimmed))m_closeRequested=true;
        Design::Tooltip(Tr("Close"));
        ImGui::EndMenuBar();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}
void Workspace::MainMenus()
{
    if(ImGui::BeginMenu(Tr("File"))) {
        if(ImGui::MenuItem(Tr("New scene..."),nullptr))RequestSceneFile(false);
        if(ImGui::MenuItem(Tr("Open scene..."),"Ctrl+O"))Attempt([&]{OpenSceneDialog();});
        if(ImGui::MenuItem(Tr("Save scene as...")))RequestSceneFile(true);
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("New UI document..."))){m_uiDesigner.NewDocument();SwitchPage(3);}
        if(ImGui::MenuItem(Tr("Open UI document...")))Attempt([&]{OpenUiDialog();});
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("Save all"),"Ctrl+S"))Attempt([&]{Save();});
        if(ImGui::MenuItem(Tr("Project settings...")))m_showProjectSettings=true;
        if(ImGui::MenuItem(Tr("Show project folder")))RevealFolder(m_project);
        ImGui::Separator();
        if(ImGui::BeginMenu(Tr("Recent projects"),m_recent.size()>1)) {
            for(const auto& path:m_recent) {
                if(path==m_project)continue;
                const auto label=Utf8Text(path.filename())+"##"+Utf8Text(path);
                if(ImGui::MenuItem(label.c_str()))Attempt([&]{LaunchWorkspace(path,Utf8Path(m_cli),Utf8Path(m_sdk));});
                Design::Tooltip(Utf8Text(path).c_str());
            }
            ImGui::EndMenu();
        }
        if(ImGui::MenuItem(Tr("Project manager...")))Attempt([&]{LaunchWorkspace({},Utf8Path(m_cli),Utf8Path(m_sdk));});
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("Preferences..."),"Ctrl+,"))m_showPreferences=true;
        if(ImGui::MenuItem(Tr("Exit"),"Alt+F4"))m_closeRequested=true;
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu(Tr("Edit"))) {
        const bool scene=m_page==1;
        if(ImGui::MenuItem(Tr("Undo"),"Ctrl+Z")){if(m_page==2)m_code.Undo();else if(m_page==3)m_uiDesigner.Undo();else Undo();}
        if(ImGui::MenuItem(Tr("Redo"),"Ctrl+Y")){if(m_page==2)m_code.Redo();else if(m_page==3)m_uiDesigner.Redo();else Undo(true);}
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("Copy"),"Ctrl+C",false,scene && IsSelected()))CopySelection();
        if(ImGui::MenuItem(Tr("Paste"),"Ctrl+V",false,scene && m_copiedObject.has_value()))PasteObject();
        if(ImGui::MenuItem(Tr("Duplicate"),"Ctrl+D",false,scene && IsSelected()))DuplicateSelection();
        if(ImGui::MenuItem(Tr("Rename"),"F2",false,scene && IsSelected()))BeginRename(m_selection);
        if(ImGui::MenuItem(Tr("Delete"),"Del",false,scene && IsSelected()))DeleteSelection();
        ImGui::Separator();
        if(ImGui::BeginMenu(Tr("Add object"),scene)){AddObjectMenu();ImGui::EndMenu();}
        if(ImGui::MenuItem(Tr("Focus selection"),"F",false,scene && (IsSelected() || m_worldSelection!=WorldSelection::None)))FocusSelection();
        if(ImGui::MenuItem(Tr("Frame all objects"),"Home",false,scene))FrameAll();
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu(Tr("View"))) {
        if(ImGui::MenuItem(Tr("3D scene"),"Ctrl+1",m_page==1))SwitchPage(1);
        if(ImGui::MenuItem(Tr("Code"),"Ctrl+2",m_page==2))SwitchPage(2);
        if(ImGui::MenuItem(Tr("User interface"),"Ctrl+3",m_page==3))SwitchPage(3);
        ImGui::Separator();
        ImGui::MenuItem(Tr("Build output"),nullptr,&m_showOutput);
        ImGui::MenuItem(Tr("Scene grid"),nullptr,&m_showGrid);
        ImGui::MenuItem(Tr("Game camera gizmo"),nullptr,&m_showGameCamera);
        if(ImGui::MenuItem(Tr("Restore default layout")))RestoreLayout();
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("Full screen"),"F11",m_window.Mode()==WindowMode::Fullscreen))ToggleFullscreen();
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu(Tr("Build"))) {
        const bool busy=m_process.Busy();
        if(ImGui::MenuItem(Tr("Preview open scene"),"F6",false,!busy))Attempt([&]{Preview();});
        if(ImGui::MenuItem(Tr("Build project"),"Ctrl+B",false,!busy))Attempt([&]{Build(false);});
        if(ImGui::MenuItem(Tr("Build and play"),"F5",false,!busy))Attempt([&]{Build(true);});
        if(ImGui::MenuItem(Tr("Stop"),"Shift+F5",false,busy))m_process.Stop();
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("Toolchain..."))){m_showPreferences=true;}
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu(Tr("Help"))) {
        if(ImGui::MenuItem(Tr("Keyboard shortcuts"),"F1"))m_showShortcuts=true;
        if(ImGui::MenuItem(Tr("Concord on GitHub")))OpenExternal(L"https://github.com/datatype-tech/concord");
        ImGui::Separator();
        if(ImGui::MenuItem(Tr("About Concord Flash")))m_showAbout=true;
        ImGui::EndMenu();
    }
}
void Workspace::CommandBar()
{
    auto* viewport=ImGui::GetMainViewport();
    const float unit=ImGui::GetFontSize(),frame=ImGui::GetFrameHeight(),height=std::round(frame+unit*0.6f);
    const ImVec4 title=ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);
    const ImVec4 panel=ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    const ImVec4 command{title.x+(panel.x-title.x)*0.55f,title.y+(panel.y-title.y)*0.55f,title.z+(panel.z-title.z)*0.55f,1.0f};
    ImGui::PushStyleColor(ImGuiCol_WindowBg,command);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{std::round(unit*0.6f),std::round((height-frame)*0.5f)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{std::round(unit*0.25f),0});
    if(ImGui::BeginViewportSideBar("##CommandBar",viewport,ImGuiDir_Up,height,BarFlags)) {
        const ImVec2 barOrigin=ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddLine(barOrigin,{barOrigin.x+ImGui::GetWindowWidth(),barOrigin.y},ImGui::GetColorU32(ImGuiCol_Border));
        const float width=ImGui::GetWindowWidth();
        const bool workspaceLabels=width>unit*34;
        const bool transportLabels=width>unit*58;
        const Segment pages[]={{"cube",workspaceLabels?Tr("3D"):nullptr},{"code",workspaceLabels?Tr("Code"):nullptr},{"grid",workspaceLabels?Tr("UI"):nullptr}};
        const int clicked=Design::Segmented("workspaces",pages,3,m_page-1);
        if(clicked>=0)SwitchPage(clicked+1);
        if(!workspaceLabels) {
            const auto position=ImGui::GetItemRectMin();
            if(ImGui::IsMouseHoveringRect(position,ImGui::GetItemRectMax()))
                ImGui::SetTooltip("%s",Tr("3D scene  /  Code  /  User interface"));
        }
        ImGui::SameLine();VerticalDivider(frame);ImGui::SameLine();
        if(Design::Ghost("Save all (Ctrl+S)","save"))Attempt([&]{Save();});
        ImGui::SameLine();
        if(Design::Ghost("Undo (Ctrl+Z)","undo")){if(m_page==2)m_code.Undo();else if(m_page==3)m_uiDesigner.Undo();else Undo();}
        ImGui::SameLine();
        if(Design::Ghost("Redo (Ctrl+Y)","redo")){if(m_page==2)m_code.Redo();else if(m_page==3)m_uiDesigner.Redo();else Undo(true);}
        const bool busy=m_process.Busy();
        const auto& style=ImGui::GetStyle();
        const auto labeled=[&](const char* text) {
            return unit+style.FramePadding.x*3.1f+ImGui::CalcTextSize(text).x;
        };
        const float previewWidth=transportLabels?labeled(Tr("Preview")):frame;
        const float buildWidth=transportLabels?labeled(Tr("Build")):frame;
        const float playWidth=transportLabels?labeled(Tr("Play")):frame;
        const float busyWidth=busy && width>unit*64?unit*1.3f+ImGui::CalcTextSize(Tr("Running...")).x:0.0f;
        const float transport=previewWidth+buildWidth+playWidth+frame+style.ItemSpacing.x*4+busyWidth+unit*0.8f;
        const float toolsEnd=ImGui::GetCursorPosX();
        const float center=(width-transport)*0.5f;
        const std::string sceneName=m_scenePath.empty()?std::string{}:Utf8Text(m_scenePath.filename());
        ImGui::SameLine(std::max(toolsEnd+unit,center));
        VerticalDivider(frame);ImGui::SameLine();
        if(busyWidth>0) {
            Design::Spinner(unit*0.45f,ImGui::GetColorU32(Design::Accent));ImGui::SameLine(0,unit*0.35f);
            ImGui::AlignTextToFramePadding();ImGui::TextColored(Design::Muted,"%s",Tr("Running..."));ImGui::SameLine();
        }
        ImGui::BeginDisabled(busy);
        if(Design::Action("Preview the open scene (F6)","eye",transportLabels?Tr("Preview"):nullptr))Attempt([&]{Preview();});
        if(transportLabels)Design::Tooltip(Tr("Preview the open scene (F6)"));
        ImGui::SameLine();
        if(Design::Ghost("Build project (Ctrl+B)","build",transportLabels?Tr("Build"):nullptr))Attempt([&]{Build(false);});
        ImGui::SameLine();
        if(Design::Action("Play the project's initial scene (F5)","play",transportLabels?Tr("Play"):nullptr,false,true))Attempt([&]{Build(true);});
        if(transportLabels)Design::Tooltip(Tr("Play the project's initial scene (F5)"));
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!busy);
        if(Design::Ghost("Stop (Shift+F5)","stop"))m_process.Stop();
        ImGui::EndDisabled();
        if(!sceneName.empty()) {
            const float nameWidth=ImGui::CalcTextSize(sceneName.c_str()).x;
            const float nameX=width-style.WindowPadding.x-nameWidth;
            if(nameX>ImGui::GetCursorPosX()+unit) {
                ImGui::SameLine(nameX);
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(Design::Muted,"%s",sceneName.c_str());
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(4);ImGui::PopStyleColor();
}
void Workspace::StatusBar()
{
    auto* viewport=ImGui::GetMainViewport();
    const float unit=ImGui::GetFontSize(),height=std::round(unit*1.6f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{std::round(unit*0.7f),std::round((height-unit)*0.5f)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    if(ImGui::BeginViewportSideBar("##StatusBar",viewport,ImGuiDir_Down,height,BarFlags)) {
        auto* draw=ImGui::GetWindowDrawList();
        const ImVec2 origin=ImGui::GetWindowPos();const float width=ImGui::GetWindowWidth();
        draw->AddLine(origin,{origin.x+width,origin.y},ImGui::GetColorU32(ImGuiCol_Separator));
        const bool busy=m_process.Busy();
        std::string right;
        if(!m_projectManager) {
            const bool dirty=m_sceneDirty || m_scriptDirty || m_uiDesigner.Dirty();
            right=std::string(dirty?Tr("Unsaved changes"):Tr("All changes saved"))+"    "+std::to_string(static_cast<int>(ImGui::GetIO().Framerate+0.5f))+" FPS";
            if(!m_scenePath.empty())right=Utf8Text(m_scenePath.filename())+"    "+right;
            if(m_page==1)right=std::to_string(m_document.objects.size())+" "+Tr("objects")+"    "+right;
        }
        right+=std::string("    ")+(IsChinese()?"中文":"English")+"    v"+EditorVersion;
        const float rightWidth=ImGui::CalcTextSize(right.c_str()).x;
        if(busy){Design::Spinner(unit*0.4f,ImGui::GetColorU32(Design::Accent));ImGui::SameLine(0,unit*0.4f);}
        else if(!m_projectManager){Design::Image("console",unit*0.9f,ImGui::GetColorU32(ImGuiCol_TextDisabled));ImGui::SameLine(0,unit*0.4f);}
        if(!m_projectManager) {
            if(Design::Ghost(m_showOutput?"Hide output":"Show output",nullptr,m_showOutput?Tr("Hide output"):Tr("Show output"),m_showOutput,unit*1.25f))
                m_showOutput=!m_showOutput;
            ImGui::SameLine(0,unit);
        }
        const float available=width-ImGui::GetCursorPosX()-rightWidth-unit*2;
        if(available>unit*3) {
            ImGui::PushClipRect(ImGui::GetCursorScreenPos(),{ImGui::GetCursorScreenPos().x+available,origin.y+height},true);
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text),"%s",m_status.c_str());
            ImGui::PopClipRect();
        }
        draw->AddText({origin.x+width-rightWidth-unit*0.7f,origin.y+(height-unit)*0.5f},ImGui::GetColorU32(ImGuiCol_TextDisabled),right.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar(3);ImGui::PopStyleColor();
}
}
