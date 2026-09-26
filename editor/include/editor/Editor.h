// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITOR_H
#define CONCORD_EDITOR_H
#include "editor/BuildProcess.h"
#include "editor/SceneDocument.h"
#include "editor/NativeDialogs.h"
#include "editor/WorkspaceLayouts.h"
#include "editor/ProjectDocument.h"
#include "editor/UiDesigner.h"
#include <Concord/CApplication.h>
#include <Concord/CCodeEditor.h>
#include <Concord/CUiToolkit.h>
#include <deque>

namespace Concord::Editor {
/** Native docking workspace sharing one live Scene with the Vulkan renderer. */
class Workspace {
public:
    Workspace(Game& game,Window& window,std::filesystem::path cli,std::filesystem::path sdk,bool projectManager=false);
    ~Workspace();
    void Open(const std::filesystem::path& project);
    void Tick();
    void Save();
    Scene& LiveScene() {return m_scene;}
private:
    void DockLayout();
    void Toolbar();
    void Home();
    void SwitchPage(int page);
    void FileBrowser();
    void CloseDialog();
    void RememberProject();
    void Preferences();
    void Hierarchy();
    void Inspector();
    void Viewport();
    void Scripts();
    void Console();
    void ProjectDialog();
    void Synchronize();
    void Checkpoint();
    void Undo(bool redo=false);
    void OpenScript(const std::filesystem::path& file);
    void SaveScript();
    void Build(bool run);
    void AddBox();
    void DeleteSelection();
    void DuplicateSelection();
    void FocusSelection();
    void UpdateCamera();
    void Pick(ImVec2 origin,ImVec2 size);
    void RecordEdit();
    bool IsSelected() const;
    void Report(const std::exception& error);
    template<class Fn> void Attempt(Fn action) {try {action();} catch(const std::exception& error) {Report(error);}}

    Game& m_game;
    Window& m_window;
    Scene m_scene;
    EntityHandle m_camera;
    std::vector<EntityHandle> m_entities;
    SceneDocument m_document;
    CodeEditor m_code;
    BuildProcess m_process;
    std::filesystem::path m_project,m_script;
    std::filesystem::path m_preferences;
    std::vector<std::filesystem::path> m_recent;
    std::string m_layoutPath,m_status="Ready",m_lastSaved,m_sourceSaved,m_pendingCreate;
    std::deque<std::string> m_undo,m_redo;
    std::string m_editBefore,m_snapshot;
    char m_cli[1024]{},m_sdk[1024]{},m_projectInput[1024]{},m_projectName[128]="MyGame";
    int m_selection=1;
    bool m_scriptDirty=false,m_sceneDirty=false,m_resetLayout=false,m_wasBusy=false,m_codeFocused=false;
    bool m_gizmoWasUsing=false,m_local=true,m_snap=false;
    bool m_showProject=false;
    bool m_closeRequested=false,m_showOutput=false,m_compact=false;
    bool m_projectManager=false;
    int m_page=0;
    ImVec2 m_layoutSize{};
    float m_uiScale=1.0f;
    char m_managerFilter[128]{};
    char m_hierarchyFilter[128]{};
    bool m_showPreferences=false,m_showGrid=true;
    int m_newTemplate=0;
    ImGuizmo::OPERATION m_operation=ImGuizmo::TRANSLATE;
    Vec3 m_target{0,1,0},m_eye{9,7,11};
    float m_yaw=0.68573f,m_pitch=0.399f,m_distance=15.42f;
    std::string m_diagnostics;
    void RefreshScriptFiles(bool force=false);
    void NewScriptDialog();
    void NavigateDiagnostic(const std::string& line);
    std::filesystem::path m_scriptListProject,m_selectedScriptTab;
    std::vector<std::filesystem::path> m_scriptFiles,m_openScripts,m_generatedScripts;
    double m_scriptScanAt=-1;
    char m_sourceFilter[128]{},m_newScriptName[128]="NewScript.cx",m_findText[256]{},m_outputFilter[128]{};
    bool m_findVisible=false,m_focusFind=false,m_findMissing=false,m_outputFollow=true,m_outputErrorsOnly=false;
    bool m_focusSource=false;
    int m_goToLine=1;
    std::string m_newScriptError,m_consoleHiddenPrefix,m_consoleRaw;
    std::vector<std::string> m_consoleLines;
    bool m_completionVisible=false;
    std::vector<std::string> m_completions;
    void OpenLayouts();
    void SaveLayouts();
    void RestoreLayout();
    WorkspaceLayouts m_workspaceLayouts;
    int m_pendingPage=0;
    void LoadPreferences();
    void SavePreferences();
    int m_uiTheme=1;
    float m_uiRounding=8;
    void SaveScene();
    void OpenScene(const std::filesystem::path& file);
    void OpenAsset(const std::filesystem::path& file);
    void SceneFileDialog();
    void ProjectSettings();
    void SceneSettings();
    void GenerateProjectRuntime();
    void ApplyProjectSettings(const ProjectDocument& settings);
    void UseProjectEntry();
    void InitializeProject(const std::filesystem::path& project);
    void RequestSceneFile(bool copy);
    ProjectDocument m_projectConfig,m_settingsDraft;
    UiDesigner m_uiDesigner;
    std::filesystem::path m_scenePath;
    std::string m_sceneSource;
    bool m_showProjectSettings=false,m_showSceneFile=false,m_copyScene=false;
    char m_sceneName[512]="Scenes/NewScene.scene";
    char m_gameTitle[256]{},m_startupScene[512]{},m_startupUi[512]{};
    std::string m_sceneFileError;
    EntityHandle m_sun;
};
}
#endif
