// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace Concord::Editor {
namespace {
std::string Lower(std::string text)
{
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return text;
}
std::string ModifiedText(const std::filesystem::path& project)
{
    std::error_code error;
    const auto written=std::filesystem::last_write_time(project/"Main.cx",error);
    if(error)return {};
    const auto time=std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>(written));
    std::tm local{};
    if(localtime_s(&local,&time)!=0)return {};
    char text[32]{};std::strftime(text,sizeof(text),"%Y-%m-%d %H:%M",&local);
    return text;
}
bool NavigationItem(const char* icon,const char* label,bool selected)
{
    const float unit=ImGui::GetFontSize(),height=std::round(unit*2.1f),width=ImGui::GetContentRegionAvail().x;
    const auto position=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton(label,{width,height});
    const float hover=Design::Fade(ImGui::GetItemID(),ImGui::IsItemHovered());
    auto* draw=ImGui::GetWindowDrawList();
    const float rounding=ImGui::GetStyle().FrameRounding;
    if(selected)draw->AddRectFilled(position,{position.x+width,position.y+height},ImGui::GetColorU32({Design::Accent.x,Design::Accent.y,Design::Accent.z,0.18f}),rounding);
    else if(hover>0.01f)draw->AddRectFilled(position,{position.x+width,position.y+height},IM_COL32(255,255,255,static_cast<int>(14*hover)),rounding);
    if(selected)draw->AddRectFilled({position.x,position.y+height*0.28f},{position.x+std::max(2.0f,unit*0.16f),position.y+height*0.72f},ImGui::GetColorU32(Design::Accent),unit);
    const ImU32 color=selected?ImGui::GetColorU32(ImGuiCol_Text):Design::Blend(ImGui::GetColorU32({0.70f,0.72f,0.76f,1}),ImGui::GetColorU32(ImGuiCol_Text),hover);
    Design::Icon(icon,{position.x+unit*0.8f,position.y+(height-unit)*0.5f},unit,selected?ImGui::GetColorU32(Design::Accent):color);
    draw->AddText({position.x+unit*2.3f,position.y+(height-unit)*0.5f},color,label);
    return clicked;
}
}

void Workspace::Home()
{
    const auto* viewport=ImGui::GetMainViewport();
    const float unit=ImGui::GetFontSize();
    ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoDocking;
    if(ImGui::Begin("##ProjectManager",nullptr,flags)) {
        const float sidebar=std::clamp(viewport->WorkSize.x*0.2f,unit*12,unit*16);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*0.9f,unit*1.2f});
        if(ImGui::BeginChild("##sidebar",{sidebar,0},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar)) {
            Design::LogoImage(unit*3.4f);
            ImGui::Spacing();
            Design::Heading("Concord Flash");
            ImGui::TextColored(Design::Muted,"%s %s",Tr("Version"),EditorVersion);
            ImGui::Dummy({0,unit*1.4f});
            if(NavigationItem("cube",Tr("Projects"),m_homePage==0))m_homePage=0;
            if(NavigationItem("build",Tr("Toolchain"),m_homePage==1))m_homePage=1;
            const float footer=ImGui::GetFrameHeight()*2+ImGui::GetStyle().ItemSpacing.y*2+unit*0.4f;
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),ImGui::GetWindowHeight()-footer-unit*1.2f));
            if(NavigationItem("settings",Tr("Preferences"),false))m_showPreferences=true;
            if(NavigationItem("focus",Tr("About"),false))m_showAbout=true;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();ImGui::PopStyleColor();
        ImGui::SameLine(0,0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*2.2f,unit*1.6f});
        if(ImGui::BeginChild("##content",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding)) {
            if(m_homePage==0)HomeProjects(ImGui::GetContentRegionAvail().x);
            else HomeToolchain();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();ImGui::PopStyleVar(2);
}

void Workspace::HomeProjects(float width)
{
    const float unit=ImGui::GetFontSize();
    Design::Heading(Tr("Projects"));
    const float buttons=Design::ButtonWidth(Tr("Import"))+Design::ButtonWidth(Tr("New project"))+ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine();Design::AlignRight(buttons);
    ImGui::BeginDisabled(m_process.Busy());
    if(Design::Action("##import","folder",Tr("Import"))) Attempt([&]{
        const auto path=ChooseFolder(WideText(Tr("Open a Concord project folder")).c_str());
        if(!path.empty())Open(path);
    });
    ImGui::SameLine();
    if(Design::Action("##newProject","plus",Tr("New project"),false,true)) {
        if(!m_projectInput[0])std::snprintf(m_projectInput,sizeof(m_projectInput),"%s",Utf8Text(std::filesystem::current_path()/"games").c_str());
        m_showProject=true;
    }
    ImGui::EndDisabled();
    ImGui::TextColored(Design::Muted,"%s",Tr("Each project opens in its own editor window."));
    ImGui::Dummy({0,unit*0.6f});
    ImGui::SetNextItemWidth(std::min(width,unit*26));
    ImGui::InputTextWithHint("##projectSearch",Tr("Search projects by name or folder..."),m_managerFilter,sizeof(m_managerFilter));
    ImGui::Dummy({0,unit*0.4f});

    const float footer=m_process.Busy() || !m_process.Output().empty()?ImGui::GetFrameHeight()*2.6f:0.0f;
    const auto filter=Lower(m_managerFilter);
    const auto recent=m_recent;int remove=-1;size_t matches=0;
    if(ImGui::BeginChild("##projectList",{0,-footer},ImGuiChildFlags_None)) {
        auto* draw=ImGui::GetWindowDrawList();
        const float rowHeight=std::round(unit*3.5f),rounding=ImGui::GetStyle().FrameRounding+2;
        for(size_t index=0;index<recent.size();++index) {
            const auto& path=recent[index];const auto pathText=Utf8Text(path);
            if(!filter.empty() && Lower(pathText).find(filter)==std::string::npos)continue;
            ++matches;ImGui::PushID(static_cast<int>(index));
            const bool exists=std::filesystem::is_regular_file(path/"Main.cx");
            const auto position=ImGui::GetCursorScreenPos();const float rowWidth=ImGui::GetContentRegionAvail().x;
            ImGui::SetNextItemAllowOverlap();
            const bool clicked=ImGui::InvisibleButton("##card",{rowWidth,rowHeight});
            const bool hovered=ImGui::IsItemHovered();
            const float hover=Design::Fade(ImGui::GetItemID(),hovered);
            draw->AddRectFilled(position,{position.x+rowWidth,position.y+rowHeight},
                Design::Blend(IM_COL32(255,255,255,6),IM_COL32(255,255,255,16),hover),rounding);
            const float tile=unit*2.1f;
            const ImVec2 tileMin{position.x+unit*0.75f,position.y+(rowHeight-tile)*0.5f};
            draw->AddRectFilled(tileMin,{tileMin.x+tile,tileMin.y+tile},ImGui::GetColorU32({Design::Accent.x,Design::Accent.y,Design::Accent.z,exists?0.18f:0.06f}),rounding);
            Design::Icon("cube",{tileMin.x+(tile-unit*1.1f)*0.5f,tileMin.y+(tile-unit*1.1f)*0.5f},unit*1.1f,
                exists?ImGui::GetColorU32(Design::Accent):ImGui::GetColorU32(ImGuiCol_TextDisabled));
            const float textX=tileMin.x+tile+unit*0.8f;
            const auto name=Utf8Text(path.filename());
            draw->AddText({textX,position.y+rowHeight*0.5f-unit*1.05f},ImGui::GetColorU32(exists?ImGuiCol_Text:ImGuiCol_TextDisabled),name.c_str());
            const float trailing=unit*11;
            draw->PushClipRect({textX,position.y},{position.x+rowWidth-trailing,position.y+rowHeight},true);
            draw->AddText({textX,position.y+rowHeight*0.5f+unit*0.08f},ImGui::GetColorU32(ImGuiCol_TextDisabled),pathText.c_str());
            draw->PopClipRect();
            const auto modified=exists?ModifiedText(path):std::string(Tr("Folder not available"));
            const float modifiedWidth=ImGui::CalcTextSize(modified.c_str()).x;
            const float menuX=position.x+rowWidth-ImGui::GetFrameHeight()-unit*0.6f;
            draw->AddText({menuX-modifiedWidth-unit*0.8f,position.y+(rowHeight-unit)*0.5f},
                exists?ImGui::GetColorU32(ImGuiCol_TextDisabled):IM_COL32(236,150,96,255),modified.c_str());
            ImGui::SetCursorScreenPos({menuX,position.y+(rowHeight-ImGui::GetFrameHeight())*0.5f});
            if(Design::Ghost("Project actions","more"))ImGui::OpenPopup("##projectActions");
            const bool openMenu=ImGui::IsPopupOpen("##projectActions");
            if(hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))ImGui::OpenPopup("##projectActions");
            if(ImGui::BeginPopup("##projectActions")) {
                if(ImGui::MenuItem(Tr("Open"),nullptr,false,exists))Attempt([&]{Open(path);});
                if(ImGui::MenuItem(Tr("Show in Explorer"),nullptr,false,exists))RevealFolder(path);
                ImGui::Separator();
                if(ImGui::MenuItem(Tr("Remove from list")))remove=static_cast<int>(index);
                ImGui::EndPopup();
            }
            if(clicked && exists && !openMenu)Attempt([&]{Open(path);});
            if(hovered)ImGui::SetMouseCursor(exists?ImGuiMouseCursor_Hand:ImGuiMouseCursor_NotAllowed);
            ImGui::SetCursorScreenPos({position.x,position.y+rowHeight+unit*0.35f});
            ImGui::Dummy({0,0});
            ImGui::PopID();
        }
        if(matches==0) {
            const float logo=unit*4.5f;
            ImGui::Dummy({0,unit*2});
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x-logo)*0.5f);Design::LogoImage(logo);
            ImGui::Spacing();
            const char* title=filter.empty()?Tr("Your next world starts here"):Tr("No matching projects");
            const char* body=filter.empty()?Tr("Create a project or import a folder to begin."):Tr("Try a different name or folder.");
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(title).x)*0.5f);ImGui::TextUnformatted(title);
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(body).x)*0.5f);ImGui::TextColored(Design::Muted,"%s",body);
        }
    }
    ImGui::EndChild();
    if(remove>=0) {
        m_recent.erase(m_recent.begin()+remove);std::string text;
        for(const auto& path:m_recent)text+=Utf8Text(path)+"\n";
        Attempt([&]{WriteText(m_preferences/"Recent.txt",text);});
    }
    if(footer>0) {
        ImGui::Separator();
        if(m_process.Busy()) {
            Design::Spinner(unit*0.5f,ImGui::GetColorU32(Design::Accent));ImGui::SameLine();
            ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(Tr("Creating project..."));
            ImGui::SameLine();Design::AlignRight(Design::ButtonWidth(Tr("Cancel")));
            if(Design::Action("##cancelCreate","stop",Tr("Cancel")))m_process.Stop();
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(m_process.ExitCode()==0?ImVec4{0.55f,0.82f,0.55f,1}:ImVec4{0.96f,0.55f,0.45f,1},"%s",
                m_process.ExitCode()==0?Tr("Last operation completed"):Tr("Last operation failed"));
            ImGui::SameLine();
            if(ImGui::SmallButton(Tr("Copy log")))ImGui::SetClipboardText(m_process.Output().c_str());
        }
    }
}

void Workspace::HomeToolchain()
{
    const float unit=ImGui::GetFontSize();
    Design::Heading(Tr("Toolchain"));
    ImGui::TextColored(Design::Muted,"%s",Tr("Build and Play call the Concord CLI, which compiles ConcordScript against an engine SDK."));
    ImGui::Dummy({0,unit*0.8f});
    std::error_code error;
    const auto card=[&](const char* id,const char* title,const char* detail,bool ready,const char* readyText,const char* missingText,char* buffer,size_t size,bool folder) {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4{1,1,1,0.03f});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*1.1f,unit*0.9f});
        if(ImGui::BeginChild("##card",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_Borders)) {
            Design::Image(folder?"folder":"build",unit*1.1f,ImGui::GetColorU32(Design::Accent));ImGui::SameLine();
            ImGui::TextUnformatted(title);ImGui::SameLine();
            Design::AlignRight(ImGui::CalcTextSize(ready?readyText:missingText).x+unit*0.8f);
            Design::Badge(ready?readyText:missingText,ready?ImVec4{0.45f,0.80f,0.52f,1}:ImVec4{0.96f,0.62f,0.40f,1});
            ImGui::TextColored(Design::Muted,"%s",detail);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Design::ButtonWidth(Tr("Browse..."),true)-ImGui::GetStyle().ItemSpacing.x);
            bool changed=ImGui::InputText("##path",buffer,size);
            ImGui::SameLine();
            if(Design::Action("##browse","folder",Tr("Browse..."))) {
                const auto path=folder?ChooseFolder(WideText(Tr("Choose the Concord SDK folder")).c_str()):ChooseExecutable();
                if(!path.empty()){std::snprintf(buffer,size,"%s",Utf8Text(path).c_str());changed=true;}
            }
            if(changed)Attempt([&]{SavePreferences();});
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();ImGui::PopStyleColor();
        ImGui::Dummy({0,unit*0.5f});
        ImGui::PopID();
    };
    const bool cli=m_cli[0] && std::filesystem::is_regular_file(Utf8Path(m_cli),error);
    card("cli",Tr("Concord CLI"),Tr("concord.exe builds, runs and creates projects."),cli,Tr("Found"),Tr("Not found"),m_cli,sizeof(m_cli),false);
    const bool development=m_sdk[0] && std::filesystem::exists(Utf8Path(m_sdk)/"include/Concord/CUiDocument.h",error);
    const bool release=m_sdk[0] && std::filesystem::is_directory(Utf8Path(m_sdk),error);
    card("sdk",Tr("Engine SDK"),Tr("Leave empty to let the CLI download the published release."),!m_sdk[0] || release,
        development?Tr("Development SDK"):m_sdk[0]?Tr("Found"):Tr("Automatic"),Tr("Not found"),m_sdk,sizeof(m_sdk),true);
}
}
