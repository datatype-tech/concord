// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/ProjectRuntime.h"
#include <Concord/CUiDocument.h>

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace Concord::Editor {
void Workspace::SaveScene()
{
    if(m_project.empty() || m_projectManager || m_scenePath.empty())return;
    const auto text=m_document.Serialize();
    if(std::filesystem::exists(m_scenePath)!=!m_sceneSource.empty() ||
       (!m_sceneSource.empty() && ReadText(m_scenePath)!=m_sceneSource))
        throw std::runtime_error("Scene changed outside the editor; save a copy before reloading.");
    std::filesystem::create_directories(m_scenePath.parent_path());
    if(text!=m_sceneSource)WriteText(m_scenePath,text);
    m_sceneSource=text;m_lastSaved=text;m_sceneDirty=false;
}
void Workspace::OpenScene(const std::filesystem::path& file)
{
    const auto relative=Utf8Text(file.lexically_relative(m_project));
    const auto path=ResolveProjectPath(m_project,relative,true);
    if(path==m_scenePath)return;
    const auto source=ReadText(path);SceneDocument document;document.Parse(source);
    SaveScene();m_document=std::move(document);m_scenePath=path;m_sceneSource=source;
    m_lastSaved=m_document.Serialize();m_sceneDirty=false;m_undo.clear();m_redo.clear();m_selection=m_document.objects.empty()?-1:0;
    Synchronize();SwitchPage(1);m_status="Opened "+relative;
}
void Workspace::OpenAsset(const std::filesystem::path& file)
{
    const auto extension=Utf8Text(file.extension());
    if(extension==".scene")OpenScene(file);
    else if(extension==".yu") {m_uiDesigner.Load(file);SwitchPage(3);}
    else if(file.filename()=="Concord.project")m_showProjectSettings=true;
    else {OpenScript(file);SwitchPage(2);m_focusSource=true;}
}
void Workspace::GenerateProjectRuntime()
{
    if(!IsProjectEntryScript(ReadText(m_project/"Main.cx")))return;
    SaveProjectRuntime(m_project,PrepareProjectRuntime(m_project,m_projectConfig));
    if(m_script.filename()=="SceneLayout.cx" || m_script.filename()=="ProjectRuntime.cx") {
        m_code.SetText(ReadText(m_script));m_sourceSaved=m_code.GetText();m_scriptDirty=false;
    }
}
void Workspace::ApplyProjectSettings(const ProjectDocument& settings)
{
    SaveScript();SaveScene();
    if(m_uiDesigner.Dirty() && !m_uiDesigner.Save()) {
        SwitchPage(3);throw std::runtime_error("Save the interface in the UI workspace, then retry this action.");
    }
    const auto prepared=PrepareProjectRuntime(m_project,settings);
    if(IsProjectEntryScript(ReadText(m_project/"Main.cx")))SaveProjectRuntime(m_project,prepared);
    WriteText(m_project/"Concord.project",settings.Serialize());m_projectConfig=settings;
    if((m_script.filename()=="SceneLayout.cx" || m_script.filename()=="ProjectRuntime.cx") && !m_scriptDirty) {
        m_code.SetText(ReadText(m_script));m_sourceSaved=m_code.GetText();
    }
}
void Workspace::UseProjectEntry()
{
    SaveScript();const auto path=m_project/"Main.cx";const auto source=ReadText(path);
    SaveProjectRuntime(m_project,PrepareProjectRuntime(m_project,m_projectConfig));
    if(source!=ProjectEntryScript()) {
        auto backup=m_project/".editor"/"Main.before-project-entry.txt";
        for(int index=1;std::filesystem::exists(backup);++index)backup=m_project/".editor"/("Main.before-project-entry-"+std::to_string(index)+".txt");
        WriteText(backup,source);WriteText(path,ProjectEntryScript());
        if(m_script==path){m_code.SetText(ProjectEntryScript());m_sourceSaved=m_code.GetText();m_scriptDirty=false;}
    }
    GenerateProjectRuntime();m_status="Project startup enabled. Previous Main.cx was backed up in .editor.";
}
void Workspace::RequestSceneFile(bool copy)
{
    m_copyScene=copy;m_showSceneFile=true;m_sceneFileError.clear();
    std::snprintf(m_sceneName,sizeof(m_sceneName),"Scenes/%s.scene",copy?"SceneCopy":"NewScene");
}
void Workspace::SceneFileDialog()
{
    if(m_showSceneFile){ImGui::OpenPopup("Scene file");m_showSceneFile=false;}
    if(!ImGui::BeginPopupModal("Scene file",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::TextUnformatted(m_copyScene?"Save current scene as a new file":"Create a new scene");
    ImGui::SetNextItemWidth(ImGui::GetFontSize()*28);ImGui::InputText("##scenePath",m_sceneName,sizeof(m_sceneName));
    ImGui::TextDisabled("Project-relative path, ending in .scene");
    if(!m_sceneFileError.empty())ImGui::TextWrapped("%s",m_sceneFileError.c_str());
    if(Design::Action("Create scene file","save","Save",false,true)) {
        try {
            auto path=ResolveProjectPath(m_project,m_sceneName);
            if(path.extension()!=L".scene")throw std::runtime_error("Use the .scene extension for new scenes.");
            if(std::filesystem::exists(path))throw std::runtime_error("File already exists; choose a new name.");
            SceneDocument document=m_copyScene?m_document:SceneDocument{};const auto text=document.Serialize();
            if(!m_copyScene)SaveScene();
            std::filesystem::create_directories(path.parent_path());WriteText(path,text);
            m_document=std::move(document);m_scenePath=path;m_sceneSource=text;m_lastSaved=text;m_sceneDirty=false;
            m_undo.clear();m_redo.clear();m_selection=m_document.objects.empty()?-1:0;
            Synchronize();RefreshScriptFiles(true);SwitchPage(1);ImGui::CloseCurrentPopup();m_status="Saved "+std::string(m_sceneName);
        }catch(const std::exception& error){m_sceneFileError=error.what();}
    }
    ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
}
void Workspace::InitializeProject(const std::filesystem::path& project)
{
    ProjectDocument settings;settings.name=Utf8Text(project.filename());settings.startupUi="UI/Main.yu";
    auto scene=m_newTemplate==0?SceneDocument::Starter():SceneDocument{};
    std::filesystem::create_directories(project/"Scenes");std::filesystem::create_directories(project/"UI");
    WriteText(project/"Scenes/Main.scene",scene.Serialize());WriteText(project/"Concord.project",settings.Serialize());
    UiDocument ui;
    ui.elements.push_back({.id="hud",.text="",.kind=UiElementKind::Panel,.position={24,24},.size={300,132},.background=COLOR_RGBA(24,28,38,220),.rounding=14});
    ui.elements.push_back({.id="title",.parent="hud",.text=settings.name,.kind=UiElementKind::Label,.position={18,14},.size={265,34},.fontScale=1.2f});
    ui.elements.push_back({.id="fullscreen",.parent="hud",.text="Fullscreen",.position={18,66},.size={126,42},.rounding=9,.action=UiAction::ToggleFullscreenWindow});
    ui.elements.push_back({.id="quit",.parent="hud",.text="Quit",.position={156,66},.size={126,42},.rounding=9,.action=UiAction::CloseWindow});
    ui.Save(project/"UI/Main.yu");
    WriteText(project/"Main.cx",ProjectEntryScript());WriteText(project/"SceneLayout.cx",scene.ExportScript());
    WriteText(project/"ProjectRuntime.cx",ExportProjectRuntime(settings,ui.Serialize(),scene.UsesPhysics()));
}
void Workspace::ProjectSettings()
{
    if(m_projectManager)return;
    if(m_showProjectSettings) {
        m_settingsDraft=m_projectConfig;
        std::snprintf(m_gameTitle,sizeof(m_gameTitle),"%s",m_settingsDraft.name.c_str());
        std::snprintf(m_startupScene,sizeof(m_startupScene),"%s",m_settingsDraft.startupScene.c_str());
        std::snprintf(m_startupUi,sizeof(m_startupUi),"%s",m_settingsDraft.startupUi.c_str());
        ImGui::OpenPopup("Project settings");m_showProjectSettings=false;
    }
    ImGui::SetNextWindowSize({ImGui::GetFontSize()*35,0},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("Project settings",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
    ImGui::SeparatorText("Startup");
    ImGui::InputText("Game title",m_gameTitle,sizeof(m_gameTitle));
    ImGui::InputText("Initial scene",m_startupScene,sizeof(m_startupScene));
    if(ImGui::BeginCombo("Choose scene",m_startupScene)) {
        for(const auto& file:m_scriptFiles)if(file.extension()==L".scene") {
            const auto relative=Utf8Text(file.lexically_relative(m_project));
            if(ImGui::Selectable(relative.c_str(),relative==m_startupScene))std::snprintf(m_startupScene,sizeof(m_startupScene),"%s",relative.c_str());
        }
        ImGui::EndCombo();
    }
    ImGui::InputText("Startup UI",m_startupUi,sizeof(m_startupUi));
    if(ImGui::BeginCombo("Choose UI",m_startupUi[0]?m_startupUi:"None")) {
        if(ImGui::Selectable("None",!m_startupUi[0]))m_startupUi[0]=0;
        for(const auto& file:m_scriptFiles)if(file.extension()==L".yu") {
            const auto relative=Utf8Text(file.lexically_relative(m_project));
            if(ImGui::Selectable(relative.c_str(),relative==m_startupUi))std::snprintf(m_startupUi,sizeof(m_startupUi),"%s",relative.c_str());
        }
        ImGui::EndCombo();
    }
    ImGui::SeparatorText("Game window");
    ImGui::InputInt("Width",&m_settingsDraft.width);ImGui::InputInt("Height",&m_settingsDraft.height);
    ImGui::Checkbox("System title bar",&m_settingsDraft.decorated);ImGui::SameLine();ImGui::Checkbox("Resizable",&m_settingsDraft.resizable);
    ImGui::Checkbox("Start fullscreen",&m_settingsDraft.fullscreen);ImGui::SameLine();ImGui::Checkbox("VSync",&m_settingsDraft.vsync);
    int fps=static_cast<int>(m_settingsDraft.targetFps);
    if(ImGui::InputInt("Frame limit (0 = unlimited)",&fps))m_settingsDraft.targetFps=static_cast<unsigned>(std::clamp(fps,0,1000));
    ImGui::SeparatorText("Entry point");
    const bool managed=IsProjectEntryScript(ReadText(m_project/"Main.cx"));
    ImGui::TextWrapped(managed?"Play uses the initial scene, UI and window settings above.":"This project has a custom Main.cx. Enable the project entry point to make Play use these settings. The old Main.cx will be backed up in .editor.");
    if(!managed && ImGui::Button("Enable project entry point"))Attempt([&]{SaveScene();UseProjectEntry();});
    ImGui::Spacing();
    if(Design::Action("Apply project settings","check","Apply",false,true))Attempt([&]{
        auto settings=m_settingsDraft;settings.name=m_gameTitle;settings.startupScene=m_startupScene;settings.startupUi=m_startupUi;
        ApplyProjectSettings(settings);m_status="Project startup settings saved";ImGui::CloseCurrentPopup();
    });
    ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
    ImGui::TextWrapped("%s",m_status.c_str());ImGui::EndPopup();
}
}
