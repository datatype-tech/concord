// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <vector>
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
    const float unit=ImGui::GetFontSize(),height=std::round(unit*1.7f),width=ImGui::GetContentRegionAvail().x;
    const auto position=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton(label,{width,height});
    const float hover=Design::Fade(ImGui::GetItemID(),ImGui::IsItemHovered());
    auto* draw=ImGui::GetWindowDrawList();
    const float rounding=ImGui::GetStyle().FrameRounding;
    if(selected)draw->AddRectFilled(position,{position.x+width,position.y+height},ImGui::GetColorU32({Design::Accent.x,Design::Accent.y,Design::Accent.z,0.18f}),rounding);
    else if(hover>0.01f)draw->AddRectFilled(position,{position.x+width,position.y+height},IM_COL32(255,255,255,static_cast<int>(14*hover)),rounding);
    if(selected)draw->AddRectFilled(position,{position.x+3.0f,position.y+height},ImGui::GetColorU32(Design::Accent),rounding,ImDrawFlags_RoundCornersLeft);
    const ImU32 color=selected?ImGui::GetColorU32(ImGuiCol_Text):Design::Blend(ImGui::GetColorU32(ImGuiCol_TextDisabled),ImGui::GetColorU32(ImGuiCol_Text),hover);
    Design::Icon(icon,{position.x+unit*0.7f,position.y+(height-unit)*0.5f},unit*0.9f,color);
    draw->AddText({position.x+unit*2.0f,position.y+(height-unit)*0.5f},color,label);
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
        const float sidebar=std::clamp(viewport->WorkSize.x*0.18f,unit*11,unit*14);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*0.55f,unit*0.7f});
        if(ImGui::BeginChild("##sidebar",{sidebar,0},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar)) {
            const float brandHeight=std::round(unit*2.9f),logo=std::round(unit*1.85f);
            const ImVec2 brand=ImGui::GetCursorScreenPos();
            auto* edge=ImGui::GetWindowDrawList();
            Design::Logo({brand.x,brand.y+(brandHeight-logo)*0.5f},logo);
            const float textX=brand.x+logo+std::round(unit*0.5f);
            const std::string version=std::string(Tr("Version"))+" "+EditorVersion;
            edge->AddText({textX,brand.y+brandHeight*0.5f-unit*0.95f},ImGui::GetColorU32(ImGuiCol_Text),"Concord Flash");
            edge->AddText({textX,brand.y+brandHeight*0.5f+unit*0.12f},ImGui::GetColorU32(Design::Muted),version.c_str());
            ImGui::Dummy({0,brandHeight});
            const ImVec2 line=ImGui::GetCursorScreenPos();
            edge->AddLine({line.x,line.y},{line.x+ImGui::GetContentRegionAvail().x,line.y},ImGui::GetColorU32(ImGuiCol_Border));
            ImGui::Dummy({0,unit*0.45f});
            if(NavigationItem("cube",Tr("Projects"),m_homePage==0))m_homePage=0;
            if(NavigationItem("build",Tr("Toolchain"),m_homePage==1))m_homePage=1;
            const float footer=ImGui::GetFrameHeight()*2+ImGui::GetStyle().ItemSpacing.y*2;
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),ImGui::GetWindowHeight()-footer-unit));
            if(NavigationItem("settings",Tr("Preferences"),false))m_showPreferences=true;
            if(NavigationItem("focus",Tr("About"),false))m_showAbout=true;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();ImGui::PopStyleColor();
        ImGui::SameLine(0,0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*1.1f,unit*0.85f});
        if(ImGui::BeginChild("##content",{0,0},ImGuiChildFlags_AlwaysUseWindowPadding)) {
            if(m_homePage==0)HomeProjects(ImGui::GetContentRegionAvail().x);
            else HomeToolchain();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();ImGui::PopStyleVar(2);
}

void Workspace::HomeProjects(float)
{
    const float unit=ImGui::GetFontSize();
    const float spacing=ImGui::GetStyle().ItemSpacing.x;
    const float buttons=Design::ButtonWidth(Tr("Import"))+Design::ButtonWidth(Tr("New project"))+spacing;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Tr("Projects"));
    ImGui::SameLine(0,unit*1.2f);
    const float search=std::clamp(ImGui::GetContentRegionAvail().x-buttons-spacing,unit*8.0f,unit*22.0f);
    ImGui::SetNextItemWidth(search);
    ImGui::InputTextWithHint("##projectSearch",Tr("Search projects by name or folder..."),m_managerFilter,sizeof(m_managerFilter));
    ImGui::SameLine();
    Design::AlignRight(buttons);
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
    ImGui::Dummy({0,unit*0.25f});

    const float footer=m_process.Busy() || !m_process.Output().empty()?ImGui::GetFrameHeight()*2.6f:0.0f;
    const auto filter=Lower(m_managerFilter);
    const auto recent=m_recent;int remove=-1;
    std::vector<size_t> shown;
    shown.reserve(recent.size());
    for(size_t index=0;index<recent.size();++index) {
        const auto& path=recent[index];
        if(!filter.empty() && Lower(Utf8Text(path)).find(filter)==std::string::npos && Lower(Utf8Text(path.filename())).find(filter)==std::string::npos)continue;
        shown.push_back(index);
    }
    const ImGuiTableFlags tableFlags=ImGuiTableFlags_ScrollY|ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|
        ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_PadOuterX|ImGuiTableFlags_NoSavedSettings;
    const float tableHeight=std::max(unit*8.0f,ImGui::GetContentRegionAvail().y-footer);
    if(ImGui::BeginTable("##projects",4,tableFlags,{0,tableHeight})) {
        ImGui::TableSetupColumn(Tr("Project"),ImGuiTableColumnFlags_WidthStretch,0.28f);
        ImGui::TableSetupColumn(Tr("Modified"),ImGuiTableColumnFlags_WidthFixed,unit*10.5f);
        ImGui::TableSetupColumn(Tr("Folder"),ImGuiTableColumnFlags_WidthStretch,0.72f);
        ImGui::TableSetupColumn("##actions",ImGuiTableColumnFlags_WidthFixed,ImGui::GetFrameHeight());
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableHeadersRow();
        if(shown.empty()) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None,unit*3.4f);
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(filter.empty()?Tr("No projects"):Tr("No matching projects"));
            ImGui::TableSetColumnIndex(2);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Design::Muted,"%s",filter.empty()?Tr("Create a project or import a folder to begin."):Tr("Try a different name or folder."));
        }
        for(const size_t index:shown) {
            const auto& path=recent[index];
            const auto pathText=Utf8Text(path);
            const auto name=Utf8Text(path.filename());
            const bool exists=std::filesystem::is_regular_file(path/"Main.cx");
            ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if(!exists)ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            const bool clicked=ImGui::Selectable(name.c_str(),false,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowOverlap);
            if(!exists)ImGui::PopStyleColor();
            if(ImGui::IsItemHovered())ImGui::SetMouseCursor(exists?ImGuiMouseCursor_Hand:ImGuiMouseCursor_NotAllowed);
            if(ImGui::IsItemClicked(ImGuiMouseButton_Right))ImGui::OpenPopup("##projectActions");
            ImGui::TableSetColumnIndex(1);
            const auto modified=exists?ModifiedText(path):std::string(Tr("Folder not available"));
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(exists?Design::Muted:ImVec4{0.76f,0.55f,0.38f,1},"%s",modified.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Design::Muted,"%s",pathText.c_str());
            ImGui::TableSetColumnIndex(3);
            const bool menu=Design::Ghost("Project actions","more");
            if(menu)ImGui::OpenPopup("##projectActions");
            if(ImGui::BeginPopup("##projectActions")) {
                if(ImGui::MenuItem(Tr("Open"),nullptr,false,exists))Attempt([&]{Open(path);});
                if(ImGui::MenuItem(Tr("Show in Explorer"),nullptr,false,exists))RevealFolder(path);
                ImGui::Separator();
                if(ImGui::MenuItem(Tr("Remove from list")))remove=static_cast<int>(index);
                ImGui::EndPopup();
            }
            if(clicked && exists && !menu && !ImGui::IsPopupOpen("##projectActions"))Attempt([&]{Open(path);});
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
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
    Design::Eyebrow(Tr("Toolchain"));
    ImGui::TextColored(Design::Muted,"%s",Tr("Build and Play call the Concord CLI, which compiles ConcordScript against an engine SDK."));
    ImGui::Dummy({0,unit*0.45f});
    std::error_code error;
    const auto card=[&](const char* id,const char* title,const char* detail,bool ready,const char* readyText,const char* missingText,char* buffer,size_t size,bool folder) {
        ImGui::PushID(id);
        ImGui::Separator();
        ImGui::Dummy({0,unit*0.35f});
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(title);
        ImGui::SameLine();
        Design::AlignRight(ImGui::CalcTextSize(ready?readyText:missingText).x+unit);
        Design::Badge(ready?readyText:missingText,ready?Design::Accent:ImVec4{0.86f,0.62f,0.38f,1});
        ImGui::TextColored(Design::Muted,"%s",detail);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Design::ButtonWidth(Tr("Browse..."),true)-ImGui::GetStyle().ItemSpacing.x);
        bool changed=ImGui::InputText("##path",buffer,size);
        ImGui::SameLine();
        if(Design::Action("##browse","folder",Tr("Browse..."))) {
            const auto path=folder?ChooseFolder(WideText(Tr("Choose the Concord SDK folder")).c_str()):ChooseExecutable();
            if(!path.empty()){std::snprintf(buffer,size,"%s",Utf8Text(path).c_str());changed=true;}
        }
        if(changed)Attempt([&]{SavePreferences();});
        ImGui::Dummy({0,unit*0.45f});
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
