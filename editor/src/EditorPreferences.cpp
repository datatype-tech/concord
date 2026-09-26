// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace Concord::Editor {
void Workspace::LoadPreferences()
{
    const auto file=m_preferences/"Preferences.txt";
    if(std::filesystem::exists(file)) {
        std::istringstream input(ReadText(file));
        int version=0,theme=1;float scale=1,rounding=8;std::string cli,sdk;
        input>>version>>theme>>scale;
        if(version==2)input>>rounding;
        if(input>>std::quoted(cli)>>std::quoted(sdk); input && (version==1 || version==2) && std::isfinite(scale) && std::isfinite(rounding)) {
            m_uiTheme=std::clamp(theme,0,3);m_uiScale=std::clamp(scale,0.85f,1.6f);m_uiRounding=std::clamp(rounding,0.0f,16.0f);
            if(!cli.empty() && std::filesystem::is_regular_file(Utf8Path(cli)))std::snprintf(m_cli,sizeof(m_cli),"%s",cli.c_str());
            if(!sdk.empty() && std::filesystem::is_directory(Utf8Path(sdk)))std::snprintf(m_sdk,sizeof(m_sdk),"%s",sdk.c_str());
        }
    }
    m_game.Toolkit()->SetAppearance({.theme=static_cast<UiToolkitTheme>(m_uiTheme),.scale=m_uiScale,.rounding=m_uiRounding});
}
void Workspace::SavePreferences()
{
    std::ostringstream text;
    text<<2<<' '<<m_uiTheme<<' '<<m_uiScale<<' '<<m_uiRounding<<'\n'<<std::quoted(m_cli)<<'\n'<<std::quoted(m_sdk)<<'\n';
    WriteText(m_preferences/"Preferences.txt",text.str());
}
void Workspace::Preferences()
{
    if(m_showPreferences){ImGui::OpenPopup("Preferences");m_showPreferences=false;}
    const float unit=ImGui::GetFontSize();
    ImGui::SetNextWindowSize({std::min(unit*36,ImGui::GetMainViewport()->Size.x-40),0},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Preferences",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SeparatorText("Appearance");
        const char* themes[]={"So Dark / Blue","Darcula Darker","Photoshop","Gray Variations"};
        bool changed=ImGui::Combo("Theme",&m_uiTheme,themes,4);
        changed=ImGui::SliderFloat("Interface scale",&m_uiScale,0.85f,1.6f,"%.2fx")||changed;
        changed=ImGui::SliderFloat("Corner radius",&m_uiRounding,0.0f,16.0f,"%.0f px")||changed;
        if(changed) {
            m_game.Toolkit()->SetAppearance({.theme=static_cast<UiToolkitTheme>(m_uiTheme),.scale=m_uiScale,.rounding=m_uiRounding});
            Attempt([&]{SavePreferences();});
        }
        ImGui::TextColored(Design::Muted,"Font and controls scale together with your display.");
        ImGui::Spacing();ImGui::SeparatorText("Build tools");
        ImGui::TextWrapped("Build and Play use these paths. An empty SDK path downloads the published release automatically.");
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##cli","concord.exe",m_cli,sizeof(m_cli));
        if(Design::Action("Browse CLI","folder","Choose CLI")) {
            auto path=ChooseExecutable();if(!path.empty())std::snprintf(m_cli,sizeof(m_cli),"%s",Utf8Text(path).c_str());
        }
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##sdk","SDK directory (optional)",m_sdk,sizeof(m_sdk));
        if(Design::Action("Browse SDK","folder","Choose SDK")) {
            auto path=ChooseFolder(L"Choose Concord SDK");if(!path.empty())std::snprintf(m_sdk,sizeof(m_sdk),"%s",Utf8Text(path).c_str());
        }
        ImGui::Spacing();ImGui::Separator();
        if(Design::Action("Close preferences","check","Done",false,true))Attempt([&]{SavePreferences();ImGui::CloseCurrentPopup();});
        ImGui::EndPopup();
    }
}
}
