// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace Concord::Editor {
void Workspace::Home()
{
    const auto* viewport=ImGui::GetMainViewport();
    const float unit=ImGui::GetFontSize();
    ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*1.5f,unit*1.2f});
    if(ImGui::Begin("Project manager",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)) {
        Design::Image("brand",unit*2.6f,ImGui::GetColorU32(Design::Accent));ImGui::SameLine(0,unit);
        ImGui::BeginGroup();Design::Heading("Projects");ImGui::TextColored(Design::Muted,"CONCORD FLASH  /  WORKSPACE");ImGui::EndGroup();
        ImGui::Dummy({0,unit});
        ImGui::BeginDisabled(m_process.Busy());
        if(Design::Action("New project","plus","New project",false,true)) {
            if(!m_projectInput[0])std::snprintf(m_projectInput,sizeof(m_projectInput),"%s",Utf8Text(std::filesystem::current_path()/"games").c_str());
            m_showProject=true;
        }
        ImGui::SameLine();
        if(Design::Action("Import existing project","folder","Import"))Attempt([&]{auto path=ChooseFolder(L"Open Concord project");if(!path.empty())Open(path);});
        ImGui::EndDisabled();ImGui::SameLine();
        if(Design::Action("Manager preferences","settings","Preferences"))m_showPreferences=true;
        ImGui::Dummy({0,unit*0.5f});
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##projectSearch","Search projects by name or folder...",m_managerFilter,sizeof(m_managerFilter));
        ImGui::Dummy({0,unit*0.5f});
        std::string filter=m_managerFilter;
        std::transform(filter.begin(),filter.end(),filter.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        const auto recent=m_recent;int remove=-1;size_t matches=0;
        const auto tableFlags=ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_ScrollY;
        if(ImGui::BeginTable("Project library",2,tableFlags,{0,std::max(unit*8,ImGui::GetContentRegionAvail().y-unit*5)})) {
            ImGui::TableSetupColumn("PROJECT",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,unit*6.7f);
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            for(size_t i=0;i<recent.size();++i) {
                const auto& path=recent[i];std::string pathText=Utf8Text(path),search=pathText;
                std::transform(search.begin(),search.end(),search.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                if(!filter.empty() && search.find(filter)==std::string::npos)continue;
                ++matches;ImGui::PushID(static_cast<int>(i));
                const bool exists=std::filesystem::is_regular_file(path/"Main.cx");
                ImGui::TableNextRow(0,unit*4.0f);ImGui::TableSetColumnIndex(0);
                ImGui::Dummy({0,unit*0.25f});
                Design::Image("cube",unit*1.6f,ImGui::GetColorU32(exists?ImGuiCol_CheckMark:ImGuiCol_TextDisabled));ImGui::SameLine(0,unit*0.6f);
                ImGui::BeginGroup();ImGui::TextUnformatted(Utf8Text(path.filename()).c_str());
                ImGui::TextColored(Design::Muted,"%s",pathText.c_str());
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",pathText.c_str());
                if(!exists)ImGui::TextColored({0.95f,0.62f,0.38f,1},"Folder not available");
                ImGui::EndGroup();ImGui::TableSetColumnIndex(1);ImGui::Dummy({0,unit*0.45f});
                ImGui::BeginDisabled(!exists);
                if(Design::Action("Open project","arrow","Open"))Attempt([&]{Open(path);});
                ImGui::EndDisabled();ImGui::SameLine();
                if(Design::Action("Project actions","more"))ImGui::OpenPopup("Project actions");
                if(ImGui::BeginPopup("Project actions")) {
                    if(ImGui::MenuItem("Open containing folder"))RevealFolder(path);
                    if(ImGui::MenuItem("Remove from recent list"))remove=static_cast<int>(i);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            if(matches==0) {
                ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Dummy({0,unit});
                ImGui::TextUnformatted(filter.empty()?"No projects yet. Create a project or import a folder.":"No projects match this search.");
            }
            ImGui::EndTable();
        }
        if(remove>=0) {
            m_recent.erase(m_recent.begin()+remove);std::string text;
            for(const auto& path:m_recent)text+=Utf8Text(path)+"\n";
            Attempt([&]{WriteText(m_preferences/"Recent.txt",text);});
        }
        ImGui::Spacing();ImGui::TextColored(Design::Muted,"%zu projects  /  Each project opens in its own editor window.",matches);
        if(m_process.Busy()) {
            ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),{-unit*7,5},"");ImGui::SameLine();
            if(Design::Action("Cancel creation","stop","Cancel"))m_process.Stop();
        }
        if(!m_process.Output().empty() && ImGui::CollapsingHeader("Activity"))ImGui::TextWrapped("%s",m_process.Output().c_str());
    }
    ImGui::End();ImGui::PopStyleVar();
}
}
