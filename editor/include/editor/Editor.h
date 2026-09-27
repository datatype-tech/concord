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
#include <optional>

namespace Concord::Editor {
/** Engine release shown in the About dialog and the project manager. */
inline constexpr const char* EditorVersion="1.0.0";

/** Presets offered by the Add object menus of the hierarchy and viewport. */
enum class ObjectPreset { Cube, Plane, Ground, Wall, StaticBody, DynamicBody };

/** Native docking workspace sharing one live Scene with the Vulkan renderer. */
class Workspace {
public:
    Workspace(Game& game,Window& window,std::filesystem::path cli,std::filesystem::path sdk,bool projectManager=false);
    ~Workspace();
    void Open(const std::filesystem::path& project);
    /** Opens the project containing a .scene, .yu or script file, then that file in its workspace. */
    void OpenFile(const std::filesystem::path& file);
    void Tick();
    void Save();
    Scene& LiveScene() {return m_scene;}
private:
    enum class WorldSelection { None, Camera, Sun, Sky, Clouds };
    void SelectWorld(WorldSelection selection);
    /** Launches game.exe immediately when the binary is current; otherwise builds, then runs. */
    void LaunchGame(const std::filesystem::path& scene);
    bool CachedGameReady() const;
    void RememberSdkStamp() const;
    void DockLayout();
    void Toolbar();
    void TitleBar();
    void MainMenus();
    void CommandBar();
    void StatusBar();
    void Home();
    void HomeProjects(float width);
    void HomeToolchain();
    void HomeAbout();
    void SwitchPage(int page);
    void FileBrowser();
    void CloseDialog();
    void AboutDialog();
    void ShortcutsDialog();
    void ScreenTransition();
    void ToggleFullscreen();
    void RememberProject();
    void Preferences();
    void ApplyLanguage();
    void Hierarchy();
    void Inspector();
    void ObjectInspector();
    void Viewport();
    void ViewportOverlay(ImVec2 origin,ImVec2 size);
    void ViewportContextMenu();
    void DrawSceneGizmos(ImDrawList& draw,const Mat4& view,const Mat4& projection,ImVec2 origin,ImVec2 size);
    void Scripts();
    void Console();
    void ProjectDialog();
    void Synchronize();
    void Checkpoint();
    void Undo(bool redo=false);
    void OpenScript(const std::filesystem::path& file);
    void SaveScript();
    void Build(bool run);
    /** Saves, then launches the scene open in the viewport rather than the project's initial scene. */
    void Preview();
    void AddBox();
    void AddObject(ObjectPreset preset);
    void AddObjectMenu();
    void DeleteSelection();
    void DuplicateSelection();
    void CopySelection();
    void PasteObject();
    void BeginRename(int index);
    void FocusSelection();
    void FrameAll();
    void UpdateCamera();
    void Pick(ImVec2 origin,ImVec2 size);
    void RecordEdit();
    bool IsSelected() const;
    bool ToolchainReady(bool build);
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
    /** Generated startup files were swapped for the open scene and must be restored when the run ends. */
    bool m_restoreRuntimeAfterRun=false;
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
    /** Run and Preview open under the Concord bar. Build never does. */
    bool m_wrapPlay=true;
    int m_uiLanguage=0;
    float m_uiRounding=3;
    void SaveScene();
    void OpenScene(const std::filesystem::path& file);
    void RequestOpenScene(const std::filesystem::path& file);
    void SceneSwitchDialog();
    void OpenSceneDialog();
    void OpenUiDialog();
    void OpenAsset(const std::filesystem::path& file);
    void SceneFileDialog();
    void ProjectSettings();
    void SceneSettings();
    /** Copies a chosen equirectangular image into the project and assigns it as the sky. */
    void AddSkybox();
    /** Clears a custom sky image and restores the analytic sky colours. */
    void UseDefaultSky();
    /** Applies the built-in volumetric cloud preset and opens that section. */
    void UseDefaultClouds();
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
    std::filesystem::path m_pendingScene;
    bool m_askSceneSwitch=false,m_showAbout=false,m_showShortcuts=false,m_showToolchain=false;
    bool m_selectWorldTab=false,m_showGameCamera=true,m_contextMenu=false;
    WorldSelection m_worldSelection=WorldSelection::None;
    bool m_focusWorldSection=false;
    std::optional<SceneObject> m_copiedObject;
    int m_renaming=-1;
    char m_renameBuffer[256]{};
    bool m_focusRename=false;
    int m_homePage=0;
    float m_snapTranslate=0.5f,m_snapRotate=15.0f,m_snapScale=0.1f;
    double m_transitionStart=-10.0;
    ImVec2 m_rightPress{};
    std::string m_createError;
    int m_lastInspected=-2;
    bool m_orbiting=false,m_panning=false;
};
}
#endif
