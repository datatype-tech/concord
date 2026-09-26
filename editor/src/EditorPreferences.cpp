// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include "editor/PropertyGrid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace Concord::Editor {
void Workspace::ApplyLanguage()
{
    SetLanguage(static_cast<LanguagePreference>(std::clamp(m_uiLanguage,0,2)));
    m_window.SetTitle(m_projectManager||m_project.empty()?std::string("Concord Flash - ")+Tr("Project Manager"):Utf8Text(m_project.filename())+" - Concord Flash");
}
void Workspace::LoadPreferences()
{
    const auto file=m_preferences/"Preferences.txt";
    if(std::filesystem::exists(file)) {
        std::istringstream input(ReadText(file));
        int version=0,theme=1,language=0;float scale=1,rounding=6;std::string cli,sdk;
        input>>version>>theme>>scale;
        if(version>=2)input>>rounding;
        if(version>=3)input>>language;
        if(input>>std::quoted(cli)>>std::quoted(sdk); input && version>=1 && version<=4 && std::isfinite(scale) && std::isfinite(rounding)) {
            if(version==3)language=language==1?2:0;
            m_uiTheme=std::clamp(theme,0,3);m_uiScale=std::clamp(scale,0.85f,1.6f);m_uiRounding=std::clamp(rounding,0.0f,16.0f);
            m_uiLanguage=std::clamp(language,0,2);
            if(!cli.empty() && std::filesystem::is_regular_file(Utf8Path(cli)))std::snprintf(m_cli,sizeof(m_cli),"%s",cli.c_str());
            if(!sdk.empty() && std::filesystem::is_directory(Utf8Path(sdk)))std::snprintf(m_sdk,sizeof(m_sdk),"%s",sdk.c_str());
        }
    }
    ApplyLanguage();
    m_game.Toolkit()->SetAppearance({.theme=static_cast<UiToolkitTheme>(m_uiTheme),.scale=m_uiScale,.rounding=m_uiRounding});
}
void Workspace::SavePreferences()
{
    std::ostringstream text;
    text<<4<<' '<<m_uiTheme<<' '<<m_uiScale<<' '<<m_uiRounding<<' '<<m_uiLanguage<<'\n'<<std::quoted(m_cli)<<'\n'<<std::quoted(m_sdk)<<'\n';
    WriteText(m_preferences/"Preferences.txt",text.str());
}
void Workspace::Preferences()
{
    if(m_showToolchain){m_showPreferences=true;m_showToolchain=false;}
    if(m_showPreferences){ImGui::OpenPopup("###Preferences");m_showPreferences=false;}
    if(!Design::BeginDialog(TrId("Preferences","Preferences").c_str(),34))return;
    Design::Heading(Tr("Preferences"));
    ImGui::SeparatorText(Tr("Appearance"));
    bool appearance=false;
    if(PropertyGrid::Begin("##appearance")) {
        const char* themes[]={"Blue night","Concord Flash","Graphite","Gray"};
        appearance|=PropertyGrid::Combo("Theme",m_uiTheme,themes,4);
        const char* languages[]={"Follow Windows","English","简体中文"};
        int language=m_uiLanguage;
        if(PropertyGrid::Combo("Language",language,languages,3)) {
            m_uiLanguage=std::clamp(language,0,2);ApplyLanguage();Attempt([&]{SavePreferences();});
        }
        appearance|=PropertyGrid::Slider("Interface scale",m_uiScale,0.85f,1.6f,"%.2fx");
        appearance|=PropertyGrid::Slider("Corner radius",m_uiRounding,0.0f,16.0f,"%.0f px");
        PropertyGrid::End();
    }
    if(appearance) {
        m_game.Toolkit()->SetAppearance({.theme=static_cast<UiToolkitTheme>(m_uiTheme),.scale=m_uiScale,.rounding=m_uiRounding});
        Attempt([&]{SavePreferences();});
    }
    ImGui::SeparatorText(Tr("Toolchain"));
    std::error_code error;
    const bool cliFound=m_cli[0] && std::filesystem::is_regular_file(Utf8Path(m_cli),error);
    ImGui::TextUnformatted(Tr("Concord CLI"));ImGui::SameLine();
    Design::Badge(cliFound?Tr("Found"):Tr("Not found"),cliFound?ImVec4{0.45f,0.80f,0.52f,1}:ImVec4{0.96f,0.62f,0.40f,1});
    const float browse=Design::ButtonWidth(Tr("Browse..."))+ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-browse);
    ImGui::InputTextWithHint("##cli","concord.exe",m_cli,sizeof(m_cli));
    ImGui::SameLine();
    if(Design::Action("##browseCli","folder",Tr("Browse..."))) {
        auto path=ChooseExecutable();if(!path.empty())std::snprintf(m_cli,sizeof(m_cli),"%s",Utf8Text(path).c_str());
    }
    ImGui::TextUnformatted(Tr("Engine SDK"));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-browse);
    ImGui::InputTextWithHint("##sdk",Tr("Automatic (published release)"),m_sdk,sizeof(m_sdk));
    ImGui::SameLine();
    if(Design::Action("##browseSdk","folder",Tr("Browse..."))) {
        auto path=ChooseFolder(WideText(Tr("Choose the Concord SDK folder")).c_str());if(!path.empty())std::snprintf(m_sdk,sizeof(m_sdk),"%s",Utf8Text(path).c_str());
    }
    ImGui::PushTextWrapPos();
    ImGui::TextColored(Design::Muted,"%s",Tr("Build and Play use these paths. An empty SDK path lets the CLI download the published release."));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    Design::AlignRight(Design::ButtonWidth(Tr("Done")));
    if(Design::Action("##closePreferences","check",Tr("Done"),false,true))Attempt([&]{SavePreferences();ImGui::CloseCurrentPopup();});
    ImGui::EndPopup();
}
}
