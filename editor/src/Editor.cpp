// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include "editor/ProjectRuntime.h"
#include <Concord/CCamera.h>
#include <Concord/CLight.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <cstdlib>

namespace Concord::Editor {
Workspace::Workspace(Game& game,Window& window,std::filesystem::path cli,std::filesystem::path sdk,bool projectManager):m_game(game),m_window(window),m_projectManager(projectManager)
{
    Design::Init(m_game.Toolkit());
    std::snprintf(m_cli,sizeof(m_cli),"%s",Utf8Text(cli).c_str());
    std::snprintf(m_sdk,sizeof(m_sdk),"%s",Utf8Text(sdk).c_str());
    m_camera=m_scene.Spawn<Object::Camera>({.position=m_eye,.target=m_target});
    m_sun=m_scene.Spawn<Object::SunLight>({.elevationDegrees=55,.azimuthDegrees=220,.intensity=3});
    const auto* local=_wgetenv(L"LOCALAPPDATA");
    m_preferences=(local?std::filesystem::path(local):std::filesystem::temp_directory_path())/"ConcordEditor";
    std::filesystem::create_directories(m_preferences);
    LoadPreferences();
    if(std::filesystem::exists(m_preferences/"Recent.txt")) {
        std::istringstream input(ReadText(m_preferences/"Recent.txt"));std::string line;
        while(m_recent.size()<12 && std::getline(input,line))if(!line.empty())m_recent.push_back(Utf8Path(line));
    }
    m_status=Tr("Ready");
    m_window.SetCloseRequestHandler([this]{m_closeRequested=true;return false;});
}
Workspace::~Workspace()
{
    m_window.SetCloseRequestHandler({});
    try { SaveLayouts(); } catch(...) {}
    try {if(!m_project.empty() && m_uiDesigner.Dirty())m_uiDesigner.SaveRecovery(m_project/".editor/UI.recovery.yu");}catch(...) {}
    try {
        if(!m_project.empty() && (m_sceneDirty || m_scriptDirty)) {
            auto recovery=m_project/".editor";std::filesystem::create_directories(recovery);
            if(m_sceneDirty) WriteText(recovery/"Recovery.scene",m_document.Serialize());
            if(m_scriptDirty) WriteText(recovery/(m_script.filename().string()+".recovery"),m_code.GetText());
        }
    } catch(...) {}
}
void Workspace::Report(const std::exception& error) {m_status=error.what();}
void Workspace::Open(const std::filesystem::path& requested)
{
    const auto project=ResolveProjectPath(requested,"Main.cx",true).parent_path();
    if(!std::filesystem::is_regular_file(project/"Main.cx")) throw std::runtime_error(Tr("Select a CLI project containing Main.cx"));
    if(m_projectManager) {
        LaunchWorkspace(project,Utf8Path(m_cli),Utf8Path(m_sdk));m_project=project;RememberProject();
        m_status=std::string(Tr("Opened"))+" "+Utf8Text(project.filename());return;
    }
    ProjectDocument settings;settings.name=Utf8Text(project.filename());
    if(std::filesystem::exists(project/"Concord.project"))settings.Parse(ReadText(project/"Concord.project"));
    else if(std::filesystem::exists(project/"Scene.scene"))settings.startupScene="Scene.scene";
    settings.ValidatePaths(project);
    const auto scenePath=ResolveProjectPath(project,settings.startupScene);
    SceneDocument loaded=SceneDocument::Starter();std::string sceneSource;
    if(std::filesystem::exists(scenePath)){sceneSource=ReadText(scenePath);loaded.Parse(sceneSource);}
    auto source=ReadText(project/"Main.cx");
    if(!m_project.empty()) Save();
    SaveLayouts();
    m_project=project;m_projectConfig=settings;m_scenePath=scenePath;m_sceneSource=sceneSource;
    m_document=std::move(loaded);m_lastSaved=m_document.Serialize();
    m_sceneDirty=sceneSource.empty();m_undo.clear();m_redo.clear();m_selection=m_document.objects.empty()?-1:0;m_worldSelection=WorldSelection::None;
    m_script=project/"Main.cx";m_code.SetLanguage(false);m_code.SetReadOnly(false);m_code.SetText(source);m_sourceSaved=source;m_scriptDirty=false;
    std::filesystem::create_directories(project/".editor");
    OpenLayouts();
    m_uiDesigner.SetProjectRoot(project);
    if(!settings.startupUi.empty())m_uiDesigner.Load(ResolveProjectPath(project,settings.startupUi,true));
    m_window.SetTitle(Utf8Text(project.filename())+" - Concord Flash");
    m_status=Tr("Project opened. Ctrl+S saves everything, F5 builds and plays.");
    Synchronize();RememberProject();FrameAll();
    if(source==StarterScript()){SaveScene();WriteText(project/"Concord.project",settings.Serialize());UseProjectEntry();}
}
void Workspace::OpenFile(const std::filesystem::path& requested)
{
    const auto file=std::filesystem::absolute(requested);
    if(std::filesystem::is_directory(file)){Open(file);return;}
    if(!std::filesystem::is_regular_file(file))throw std::runtime_error(Tr("The file does not exist."));
    for(auto folder=file.parent_path();!folder.empty();folder=folder.parent_path()) {
        if(std::filesystem::is_regular_file(folder/"Main.cx")) {
            Open(folder);
            if(!m_projectManager)OpenAsset(file);
            return;
        }
        if(folder==folder.root_path())break;
    }
    throw std::runtime_error(Tr("This file is not inside a Concord project (no Main.cx in its parent folders)."));
}
void Workspace::RememberProject()
{
    std::erase(m_recent,m_project);m_recent.insert(m_recent.begin(),m_project);if(m_recent.size()>12)m_recent.resize(12);
    std::string text;for(const auto& path:m_recent)text+=Utf8Text(path)+"\n";WriteText(m_preferences/"Recent.txt",text);
}
void Workspace::SaveScript()
{
    if(m_scriptDirty) {
        if(std::filesystem::exists(m_script) && ReadText(m_script)!=m_sourceSaved)throw std::runtime_error(Tr("Source changed outside the editor; recover your edits before reloading."));
        WriteText(m_script,m_code.GetText());m_sourceSaved=m_code.GetText();m_scriptDirty=false;
    }
}
void Workspace::Save()
{
    if(m_project.empty() || m_projectManager)return;
    ApplyProjectSettings(m_projectConfig);
    SaveLayouts();m_status=Tr("Saved scripts, scene, interface and project settings");
}
void Workspace::OpenScript(const std::filesystem::path& file)
{
    if(m_script==file)return;
    auto source=ReadText(file);SaveScript();
    m_script=file;m_sourceSaved=source;m_code.SetLanguage(file.extension()==L".cpp" || file.extension()==L".h" || file.extension()==L".hpp");
    m_code.SetText(source);m_code.ClearErrors();m_scriptDirty=false;m_completionVisible=false;
    m_code.SetReadOnly(source.starts_with("// Generated by Concord Editor."));
}
void Workspace::Synchronize()
{
    m_snapshot=m_document.Serialize();
    EnvironmentSettings environment=m_document.environment;
    if(!environment.skybox.empty() && !m_scenePath.empty()) {
        const auto sky=(m_scenePath.parent_path()/Utf8Path(environment.skybox)).lexically_normal();
        environment.skybox=Utf8Text(sky);
    }
    m_scene.SetEnvironment(environment);
    if(auto* sun=m_sun.Get<LightComponent>()) {
        sun->elevationDegrees=m_document.sun.elevationDegrees;sun->azimuthDegrees=m_document.sun.azimuthDegrees;
        sun->color=m_document.sun.color;sun->intensity=m_document.sun.intensity;sun->castShadow=m_document.sun.castShadow;
    }
    while(m_entities.size()>m_document.objects.size()) {m_entities.back().Destroy();m_entities.pop_back();}
    while(m_entities.size()<m_document.objects.size())m_entities.push_back(m_scene.Spawn<Object::Box>({}));
    for(size_t i=0;i<m_entities.size();++i) {
        const auto& record=m_document.objects[i];auto& handle=m_entities[i];
        *handle.Get<Transform>()=record.transform;handle.Get<MeshRenderer>()->size=record.size;
        handle.Get<MeshRenderer>()->visible=record.visible;handle.Get<MeshRenderer>()->castShadow=record.castShadow;
        *handle.Get<Material>()=record.material;
    }
}
void Workspace::Checkpoint()
{
    m_undo.push_back(m_document.Serialize());if(m_undo.size()>64)m_undo.pop_front();m_redo.clear();m_sceneDirty=true;
}
void Workspace::Undo(bool redo)
{
    auto& from=redo?m_redo:m_undo;auto& to=redo?m_undo:m_redo;
    if(from.empty())return;
    to.push_back(m_document.Serialize());m_document.Parse(from.back());from.pop_back();
    if(!IsSelected())m_selection=m_document.objects.empty()?-1:static_cast<int>(m_document.objects.size())-1;
    if(IsSelected())m_worldSelection=WorldSelection::None;
    m_sceneDirty=m_document.Serialize()!=m_lastSaved;Synchronize();
}
bool Workspace::IsSelected() const {return m_selection>=0 && static_cast<size_t>(m_selection)<m_document.objects.size();}
void Workspace::AddBox() {AddObject(ObjectPreset::Cube);}
void Workspace::AddObject(ObjectPreset preset)
{
    if(m_document.objects.size()>=10000){m_status=Tr("A scene supports at most 10000 objects.");return;}
    Checkpoint();
    SceneObject object;object.transform.position={m_target.x,std::max(0.5f,m_target.y),m_target.z};
    const auto count=std::to_string(m_document.objects.size()+1);
    switch(preset) {
    case ObjectPreset::Cube: object.name=std::string(Tr("Cube"))+" "+count;break;
    case ObjectPreset::Plane:
        object.name=std::string(Tr("Plane"))+" "+count;object.kind=SceneObjectKind::Plane;object.size={10,0.05f,10};
        object.transform.position.y=0;object.material.albedo=COLOR_RGB(120,128,140);break;
    case ObjectPreset::Ground:
        object.name=Tr("Ground");object.kind=SceneObjectKind::StaticBox;object.size={20,0.5f,20};
        object.transform.position={m_target.x,-0.25f,m_target.z};object.material.albedo=COLOR_RGB(64,72,86);break;
    case ObjectPreset::Wall:
        object.name=std::string(Tr("Wall"))+" "+count;object.kind=SceneObjectKind::StaticBox;object.size={6,3,0.3f};
        object.transform.position.y=1.5f;object.material.albedo=COLOR_RGB(176,182,194);break;
    case ObjectPreset::StaticBody:
        object.name=std::string(Tr("Static body"))+" "+count;object.kind=SceneObjectKind::StaticBox;break;
    case ObjectPreset::DynamicBody:
        object.name=std::string(Tr("Dynamic body"))+" "+count;object.kind=SceneObjectKind::DynamicBox;
        object.transform.position.y=std::max(3.0f,m_target.y+2);object.material.albedo=COLOR_RGB(236,154,72);break;
    }
    m_document.objects.push_back(object);m_selection=static_cast<int>(m_document.objects.size())-1;m_worldSelection=WorldSelection::None;Synchronize();
    m_status=std::string(Tr("Added"))+" "+object.name;
}
void Workspace::DeleteSelection()
{
    if(!IsSelected())return;Checkpoint();
    const auto name=m_document.objects[m_selection].name;
    m_document.objects.erase(m_document.objects.begin()+m_selection);
    m_selection=std::min(m_selection,static_cast<int>(m_document.objects.size())-1);Synchronize();
    m_status=std::string(Tr("Deleted"))+" "+name;
}
void Workspace::DuplicateSelection()
{
    if(!IsSelected())return;Checkpoint();auto copy=m_document.objects[m_selection];
    copy.name=(copy.name+" "+Tr("copy")).substr(0,255);copy.transform.position.x+=1;
    m_document.objects.push_back(copy);m_selection=static_cast<int>(m_document.objects.size())-1;m_worldSelection=WorldSelection::None;Synchronize();
}
void Workspace::CopySelection()
{
    if(!IsSelected())return;
    m_copiedObject=m_document.objects[m_selection];
    m_status=std::string(Tr("Copied"))+" "+m_copiedObject->name;
}
void Workspace::PasteObject()
{
    if(!m_copiedObject || m_document.objects.size()>=10000)return;
    Checkpoint();auto copy=*m_copiedObject;copy.transform.position.x+=1;copy.transform.position.z+=1;
    m_document.objects.push_back(copy);m_selection=static_cast<int>(m_document.objects.size())-1;m_worldSelection=WorldSelection::None;Synchronize();
    m_status=std::string(Tr("Pasted"))+" "+copy.name;
}
void Workspace::BeginRename(int index)
{
    if(index<0 || static_cast<size_t>(index)>=m_document.objects.size())return;
    m_selection=index;m_renaming=index;m_focusRename=true;
    std::snprintf(m_renameBuffer,sizeof(m_renameBuffer),"%s",m_document.objects[index].name.c_str());
}
void Workspace::RecordEdit()
{
    if(ImGui::IsItemActivated()) m_editBefore=m_snapshot;
    if(ImGui::IsItemDeactivatedAfterEdit() && !m_editBefore.empty()) {
        m_undo.push_back(m_editBefore);if(m_undo.size()>64)m_undo.pop_front();m_redo.clear();m_editBefore.clear();
    }
}
bool Workspace::ToolchainReady(bool build)
{
    std::error_code error;
    if(m_cli[0] && std::filesystem::is_regular_file(Utf8Path(m_cli),error))return true;
    m_showToolchain=true;
    m_status=build?Tr("Concord CLI (concord.exe) was not found. Choose it to build and play."):Tr("Concord CLI (concord.exe) was not found.");
    return false;
}
void Workspace::SelectWorld(WorldSelection selection)
{
    m_selection=-1;m_worldSelection=selection;m_selectWorldTab=true;m_focusWorldSection=true;
}
void Workspace::Build(bool run)
{
    if(m_project.empty())return;
    if(run) {
        LaunchGame(ResolveProjectPath(m_project,m_projectConfig.startupScene,true));
        return;
    }
    if(!ToolchainReady(true))return;
    Save();m_code.ClearErrors();m_diagnostics.clear();
    std::vector<std::wstring> arguments={L"build",m_project.wstring()};
    if(m_sdk[0]) {arguments.push_back(L"--sdk");arguments.push_back(Utf8Path(m_sdk).wstring());}
    m_process.Start(Utf8Path(m_cli),arguments,m_project,false);
    m_wasBusy=true;m_status=Tr("Building the project...");
    m_showOutput=true;
}
void Workspace::ToggleFullscreen()
{
    m_window.ToggleFullscreen();
    m_transitionStart=ImGui::GetTime();
}
void Workspace::Tick()
{
    try {
        Design::SyncTheme();
        if(std::getenv("CONCORD_EDITOR_SMOKE") && !m_projectManager) {
            if(m_game.FrameCount()==25)SwitchPage(2);
            if(m_game.FrameCount()==55)SwitchPage(3);
            if(m_game.FrameCount()==90)Save();
        }
        Toolbar();
        if(m_projectManager)Home();
        else {
            DockLayout();FileBrowser();
            if(m_page==1) {Hierarchy();Inspector();Viewport();m_codeFocused=false;}
            else if(m_page==2)Scripts();
            else {m_uiDesigner.Draw();m_codeFocused=false;}
            if(m_showOutput)Console();
        }
        CloseDialog();
        if(m_showProject) {ImGui::OpenPopup("###NewProject");m_showProject=false;}
        ProjectDialog();Preferences();SceneFileDialog();ProjectSettings();SceneSwitchDialog();AboutDialog();ShortcutsDialog();
        auto& io=ImGui::GetIO();
        const bool text=io.WantTextInput;
        if(io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S,false))Attempt([&]{Save();});
        if(!m_projectManager && ImGui::IsKeyPressed(ImGuiKey_F5,false)) {
            if(io.KeyShift)m_process.Stop();else if(!m_process.Busy())Attempt([&]{Build(true);});
        }
        if(!m_projectManager && ImGui::IsKeyPressed(ImGuiKey_F6,false) && !io.KeyShift && !m_process.Busy())Attempt([&]{Preview();});
        if(!m_projectManager && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B,false) && !m_process.Busy())Attempt([&]{Build(false);});
        if(!m_projectManager && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O,false))Attempt([&]{OpenSceneDialog();});
        if(ImGui::IsKeyPressed(ImGuiKey_F11,false))ToggleFullscreen();
        if(ImGui::IsKeyPressed(ImGuiKey_F1,false))m_showShortcuts=true;
        if(!m_projectManager && m_page==3 && !text && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if(io.KeyShift)m_uiDesigner.Redo();else m_uiDesigner.Undo();
        }
        if(!m_projectManager && m_page==1 && !text && !m_codeFocused && m_renaming<0) {
            if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))Undo(io.KeyShift);
            if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))Undo(true);
            if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D,false))DuplicateSelection();
            if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C,false))CopySelection();
            if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V,false))PasteObject();
            if(!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Delete,false))DeleteSelection();
            if(ImGui::IsKeyPressed(ImGuiKey_F2,false))BeginRename(m_selection);
            if(ImGui::IsKeyPressed(ImGuiKey_Home,false))FrameAll();
        }
        if(m_wasBusy && !m_process.Busy()) {
            m_wasBusy=false;
            if(m_process.ExitCode()==0)RememberSdkStamp();
            if(m_restoreRuntimeAfterRun) {
                m_restoreRuntimeAfterRun=false;
                try {GenerateProjectRuntime();} catch(const std::exception& error) {Report(error);}
            }
            m_diagnostics=m_process.Output();
            m_status=m_process.ExitCode()==0?Tr("Process completed successfully"):Tr("Process failed; see Build output");
            if(!m_pendingCreate.empty()) {
                auto pending=Utf8Path(m_pendingCreate);m_pendingCreate.clear();
                if(m_process.ExitCode()==0) {
                    InitializeProject(pending);
                    Open(pending);
                }
            }
        }
        ScreenTransition();
    } catch(const std::exception& error) {
        if(std::getenv("CONCORD_EDITOR_SMOKE"))throw;
        Report(error);
    }
}
}
