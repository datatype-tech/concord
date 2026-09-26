// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include "editor/PropertyGrid.h"
#include "editor/ProjectRuntime.h"
#include <Concord/CUiDocument.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace Concord::Editor {
namespace {
std::string LowerExtension(const std::filesystem::path& file)
{
    auto extension=Utf8Text(file.extension());
    for(char& c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension;
}
/** Project-relative UTF-8 path, or empty when the file lies outside the project. */
std::string InsideProject(const std::filesystem::path& project,const std::filesystem::path& file)
{
    const auto relative=std::filesystem::absolute(file).lexically_normal().lexically_relative(std::filesystem::absolute(project).lexically_normal());
    if(relative.empty() || relative.begin()->string()=="..")return {};
    return Utf8Text(relative);
}
}
void Workspace::SaveScene()
{
    if(m_project.empty() || m_projectManager || m_scenePath.empty())return;
    const bool exists=std::filesystem::exists(m_scenePath);
    if(!m_sceneDirty && exists)return;
    const auto text=m_document.Serialize();
    if(exists && ReadText(m_scenePath)!=m_sceneSource)
        throw std::runtime_error(Tr("The scene changed outside the editor. Use File > Save scene as... to keep both versions."));
    std::filesystem::create_directories(m_scenePath.parent_path());
    WriteText(m_scenePath,text);
    m_sceneSource=text;m_lastSaved=text;m_sceneDirty=false;
}
void Workspace::OpenScene(const std::filesystem::path& file)
{
    const auto relative=InsideProject(m_project,file);
    if(relative.empty())throw std::runtime_error(Tr("Choose a scene inside the current project."));
    const auto path=ResolveProjectPath(m_project,relative,true);
    if(path==m_scenePath){SwitchPage(1);return;}
    const auto source=ReadText(path);SceneDocument document;document.Parse(source);
    m_document=std::move(document);m_scenePath=path;m_sceneSource=source;
    m_lastSaved=m_document.Serialize();m_sceneDirty=false;m_undo.clear();m_redo.clear();m_selection=m_document.objects.empty()?-1:0;
    Synchronize();SwitchPage(1);FrameAll();
    m_status=std::string(Tr("Opened"))+" "+relative;
}
void Workspace::RequestOpenScene(const std::filesystem::path& file)
{
    if(m_sceneDirty && file!=m_scenePath){m_pendingScene=file;m_askSceneSwitch=true;return;}
    OpenScene(file);
}
void Workspace::SceneSwitchDialog()
{
    if(m_askSceneSwitch){ImGui::OpenPopup("###SceneSwitch");m_askSceneSwitch=false;m_sceneFileError.clear();}
    if(!Design::BeginDialog(TrId("Unsaved scene","SceneSwitch").c_str(),30))return;
    Design::Heading(Tr("Save changes to the current scene?"));
    ImGui::Spacing();
    ImGui::TextColored(Design::Muted,"%s",Utf8Text(m_scenePath.lexically_relative(m_project)).c_str());
    if(!m_sceneFileError.empty()){ImGui::PushTextWrapPos();ImGui::TextColored({0.96f,0.55f,0.45f,1},"%s",m_sceneFileError.c_str());ImGui::PopTextWrapPos();}
    ImGui::Spacing();
    const float width=Design::ButtonWidth(Tr("Save"),false)+Design::ButtonWidth(Tr("Don't save"),false)+Design::ButtonWidth(Tr("Cancel"),false)+ImGui::GetStyle().ItemSpacing.x*2;
    Design::AlignRight(width);
    if(Design::Action("##saveSwitch","",Tr("Save"),false,true)) {
        try{SaveScene();OpenScene(m_pendingScene);ImGui::CloseCurrentPopup();}
        catch(const std::exception& error){m_sceneFileError=error.what();}
    }
    ImGui::SameLine();
    if(Design::Action("##discardSwitch","",Tr("Don't save"))) {
        try{m_sceneDirty=false;OpenScene(m_pendingScene);ImGui::CloseCurrentPopup();}
        catch(const std::exception& error){m_sceneDirty=true;m_sceneFileError=error.what();}
    }
    ImGui::SameLine();if(Design::Action("##cancelSwitch","",Tr("Cancel")))ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Workspace::OpenSceneDialog()
{
    if(m_project.empty())return;
    const auto folder=std::filesystem::is_directory(m_project/"Scenes")?m_project/"Scenes":m_project;
    const auto file=ChooseFile(WideText(Tr("Open scene")).c_str(),L"Concord scene (*.scene)",L"*.scene",folder);
    if(!file.empty())RequestOpenScene(file);
}
void Workspace::OpenUiDialog()
{
    if(m_project.empty())return;
    const auto folder=std::filesystem::is_directory(m_project/"UI")?m_project/"UI":m_project;
    const auto file=ChooseFile(WideText(Tr("Open UI document")).c_str(),L"Concord UI (*.yu)",L"*.yu",folder);
    if(file.empty())return;
    if(InsideProject(m_project,file).empty())throw std::runtime_error(Tr("Choose a UI document inside the current project."));
    m_uiDesigner.Load(file);SwitchPage(3);
}
void Workspace::OpenAsset(const std::filesystem::path& file)
{
    const auto extension=LowerExtension(file);
    if(extension==".scene")RequestOpenScene(file);
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
        SwitchPage(3);throw std::runtime_error(Tr("Save the interface in the UI workspace, then retry this action."));
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
    GenerateProjectRuntime();m_status=Tr("Project startup enabled. The previous Main.cx was backed up in .editor.");
}
void Workspace::RequestSceneFile(bool copy)
{
    m_copyScene=copy;m_showSceneFile=true;m_sceneFileError.clear();
    std::snprintf(m_sceneName,sizeof(m_sceneName),"Scenes/%s.scene",copy?"SceneCopy":"NewScene");
}
void Workspace::SceneFileDialog()
{
    if(m_showSceneFile){ImGui::OpenPopup("###SceneFile");m_showSceneFile=false;}
    if(!Design::BeginDialog(TrId(m_copyScene?"Save scene as":"New scene","SceneFile").c_str(),30))return;
    Design::Heading(m_copyScene?Tr("Save the current scene as a new file"):Tr("Create a new scene"));
    ImGui::Spacing();
    ImGui::TextUnformatted(Tr("File name"));
    if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool enter=ImGui::InputText("##scenePath",m_sceneName,sizeof(m_sceneName),ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextColored(Design::Muted,"%s",Tr("Relative to the project folder, ending in .scene"));
    if(!m_sceneFileError.empty()){ImGui::PushTextWrapPos();ImGui::TextColored({0.96f,0.55f,0.45f,1},"%s",m_sceneFileError.c_str());ImGui::PopTextWrapPos();}
    ImGui::Spacing();
    const float width=Design::ButtonWidth(Tr("Cancel"),false)+Design::ButtonWidth(Tr("Save"))+ImGui::GetStyle().ItemSpacing.x;
    Design::AlignRight(width);
    if(Design::Action("##cancelScene","",Tr("Cancel")))ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if(Design::Action("##createScene","save",Tr("Save"),false,true) || enter) {
        try {
            auto path=ResolveProjectPath(m_project,m_sceneName);
            if(LowerExtension(path)!=".scene")throw std::runtime_error(Tr("Use the .scene extension for new scenes."));
            if(std::filesystem::exists(path))throw std::runtime_error(Tr("That file already exists; choose another name."));
            SceneDocument document=m_copyScene?m_document:SceneDocument{};const auto text=document.Serialize();
            if(!m_copyScene)SaveScene();
            std::filesystem::create_directories(path.parent_path());WriteText(path,text);
            m_document=std::move(document);m_scenePath=path;m_sceneSource=text;m_lastSaved=text;m_sceneDirty=false;
            m_undo.clear();m_redo.clear();m_selection=m_document.objects.empty()?-1:0;
            Synchronize();RefreshScriptFiles(true);SwitchPage(1);ImGui::CloseCurrentPopup();
            m_status=std::string(Tr("Saved"))+" "+m_sceneName;
        }catch(const std::exception& error){m_sceneFileError=error.what();}
    }
    ImGui::EndPopup();
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
        ImGui::OpenPopup("###ProjectSettings");m_showProjectSettings=false;
    }
    if(!Design::BeginDialog(TrId("Project settings","ProjectSettings").c_str(),36))return;
    Design::Heading(Tr("Project settings"));
    ImGui::TextColored(Design::Muted,"%s",Tr("Play starts the game with these settings."));
    ImGui::SeparatorText(Tr("Startup"));
    if(PropertyGrid::Begin("##startup")) {
        PropertyGrid::Text("Game title",m_gameTitle,sizeof(m_gameTitle));
        PropertyGrid::Row("Initial scene");
        if(ImGui::BeginCombo("##initialScene",m_startupScene)) {
            for(const auto& file:m_scriptFiles)if(LowerExtension(file)==".scene") {
                const auto relative=Utf8Text(file.lexically_relative(m_project));
                if(ImGui::Selectable(relative.c_str(),relative==m_startupScene))std::snprintf(m_startupScene,sizeof(m_startupScene),"%s",relative.c_str());
            }
            ImGui::EndCombo();
        }
        PropertyGrid::Row("Startup UI");
        if(ImGui::BeginCombo("##startupUi",m_startupUi[0]?m_startupUi:Tr("None"))) {
            if(ImGui::Selectable(Tr("None"),!m_startupUi[0]))m_startupUi[0]=0;
            for(const auto& file:m_scriptFiles)if(LowerExtension(file)==".yu") {
                const auto relative=Utf8Text(file.lexically_relative(m_project));
                if(ImGui::Selectable(relative.c_str(),relative==m_startupUi))std::snprintf(m_startupUi,sizeof(m_startupUi),"%s",relative.c_str());
            }
            ImGui::EndCombo();
        }
        PropertyGrid::End();
    }
    ImGui::SeparatorText(Tr("Game window"));
    if(PropertyGrid::Begin("##window")) {
        PropertyGrid::Row("Resolution");
        int size[]={m_settingsDraft.width,m_settingsDraft.height};
        if(ImGui::InputInt2("##resolution",size)){m_settingsDraft.width=std::clamp(size[0],320,16384);m_settingsDraft.height=std::clamp(size[1],240,16384);}
        PropertyGrid::Check("System title bar",m_settingsDraft.decorated);
        PropertyGrid::Check("Resizable",m_settingsDraft.resizable);
        PropertyGrid::Check("Start in full screen",m_settingsDraft.fullscreen);
        PropertyGrid::Check("VSync",m_settingsDraft.vsync);
        int fps=static_cast<int>(m_settingsDraft.targetFps);
        PropertyGrid::Row("Frame limit","0 = unlimited");
        if(ImGui::InputInt("##fps",&fps,10,60))m_settingsDraft.targetFps=static_cast<unsigned>(std::clamp(fps,0,1000));
        PropertyGrid::End();
    }
    ImGui::SeparatorText(Tr("Entry point"));
    const bool managed=IsProjectEntryScript(ReadText(m_project/"Main.cx"));
    ImGui::PushTextWrapPos();
    ImGui::TextColored(Design::Muted,"%s",managed?Tr("Play uses the initial scene, UI and window settings above."):
        Tr("This project has a custom Main.cx. Enable the project entry point to make Play use these settings. The old Main.cx is backed up in .editor."));
    ImGui::PopTextWrapPos();
    if(!managed && Design::Action("##enableEntry","code",Tr("Enable project entry point")))Attempt([&]{SaveScene();UseProjectEntry();});
    ImGui::Spacing();
    const float width=Design::ButtonWidth(Tr("Cancel"),false)+Design::ButtonWidth(Tr("Apply"))+ImGui::GetStyle().ItemSpacing.x;
    Design::AlignRight(width);
    if(Design::Action("##cancelSettings","",Tr("Cancel")))ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if(Design::Action("##applySettings","check",Tr("Apply"),false,true))Attempt([&]{
        auto settings=m_settingsDraft;settings.name=m_gameTitle;settings.startupScene=m_startupScene;settings.startupUi=m_startupUi;
        ApplyProjectSettings(settings);m_status=Tr("Project startup settings saved");ImGui::CloseCurrentPopup();
    });
    ImGui::EndPopup();
}
}
