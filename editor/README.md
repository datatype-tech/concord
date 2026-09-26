# Concord Editor

Native Windows project manager, ConcordScript IDE, 3D scene workspace and UI
designer. The project manager has its own window. Opening a project starts a
separate editor with **3D**, **Code** and **UI** workspace buttons, dockable panels,
a dark theme, scalable fonts, SVG action icons and a custom window control bar.

## Build and launch

Install the engine dependencies described in the repository README. From the
`concord` repository directory, use a C++23 compiler, CMake, Ninja and
`glslangValidator` or `glslc`:

```powershell
cmake -S editor -B build/editor -G Ninja -DCMAKE_BUILD_TYPE=Release -DCONCORD_BUILD_SHADERS=ON -DCONCORD_BUILD_TESTS=ON
cmake --build build/editor --target concord_editor -j 4
.\build\editor\ConcordEditor.exe
```

In the enclosing development workspace, the existing configured build can use
`cmake --build camera-build --target concord_editor`; launch
`camera-build/editor/ConcordEditor.exe`. Keep its staged DLLs and `Assets/Shaders`
directory beside the executable, including the native UI shaders.

Current generated projects use engine APIs newer than the tagged
`v0.1.0-preview.1` SDK. After building the current engine, stage a separate local
development SDK from the enclosing workspace:

```powershell
pwsh -File concord/editor/StageSdk.ps1 `
  -BaseSdk "bin/releases/ConcordFlash-0.1.0-preview.1-win64" `
  -EngineBuild "camera-build" `
  -Destination "bin/editor-sdk"
```

This reuses the base compiler/toolchain and copies the current engine binaries,
headers, shaders and third-party notices into a separate directory. The editor
detects `bin/editor-sdk` when it contains `include/Concord/CUiDocument.h`; the
SDK path can also be selected in **Preferences**. Leaving it empty lets the CLI
download a published GitHub release, which must contain these newer APIs to
build projects created by the current editor.

## Create and run a game

1. In **Preferences**, select `concord.exe` and the development SDK.
2. Click **New project**, choose a folder and name, then choose the 3D starter or
   empty scene. **Import project** opens a project containing `Main.cx`.
3. Arrange objects in **3D**, edit behavior in **Code**, and build the interface
   in **UI**.
4. Click **Save**, **Build** or **Play**. The editor invokes the CLI in the
   background and displays its output. **Stop** ends the build or game launched
   by this editor.

Recent projects can be filtered, opened in Explorer or removed from the list.
Removing an entry leaves its files intact. Preferences include themes, interface
scale and tool locations. The custom title bar includes minimize,
maximize/restore, fullscreen and close controls.

## Project and startup files

| File | Purpose |
| --- | --- |
| `Concord.project` | Game name, initial scene, optional startup UI and window settings |
| `Main.cx` | Editable game entry point; new projects attach the generated runtime and run the game |
| `Scenes/Main.scene` | Initial authored scene in new projects |
| `UI/Main.yu` | Starter HUD with title, Fullscreen and Quit buttons |
| `SceneLayout.cx` | Generated scene construction; read-only in the editor |
| `ProjectRuntime.cx` | Generated window, scene and UI setup; read-only in the editor |
| `.editor/` | Local workspace layouts and recovery files |
| `build-cli/` | Generated C++ and built game files |

Use **Project > Project settings** to select the initial `.scene`, optional
startup `.yu`, window dimensions, system title bar, resizing, fullscreen, VSync
and target frame rate. A target of zero removes the frame limit. In the file
browser, right-click a scene and choose **Set as initial scene**, or an interface
and choose **Set as startup UI**. Opening a different scene for editing does not
change which scene Play starts.

An imported project's custom `Main.cx` remains its entry point. To make it use
the project settings, choose **Enable project entry point** in Project settings.
The previous entry script is backed up as
`.editor/Main.before-project-entry*.txt`. Runtime generation refuses to replace
unrelated user code in its generated output files.

## 3D workspace

Select objects in the viewport or Hierarchy; create boxes, thin planes, static
physics boxes and dynamic physics boxes. Ground and wall presets use boxes with
different dimensions. Duplicate, rename, hide or delete objects through GUI
actions. The Inspector edits transform, dimensions, albedo, metallic, roughness,
emission, visibility and shadow casting. Physics boxes expose friction and
bounce; dynamic boxes also expose mass and rotation locking. Physics simulation
runs in the game started by **Play**.

Move/rotate/scale gizmos support local coordinates and snapping. Right-drag
orbits, middle-drag pans, the wheel zooms, and **F** focuses the selection.
Scene settings edit the game camera, sunlight, environment, clouds, fog and
postprocessing. Camera controls include perspective/orthographic projection,
field of view and clipping planes. The game camera is independent of the
editor's orbit camera; **Use current 3D view** copies its position and target.

The `.scene` format is version 3. It stores camera, sun, environment and object
settings, including kind, visibility, shadows and physics. Version 1 and 2 files
load with defaults for their missing fields and become version 3 when saved.
Documents are limited to 10,000 objects and 8 MiB.

## Code workspace

Browse/filter project files, create `.cx` scripts and switch files using tabs.
The source editor supports ConcordScript colors, line numbers, UTF-8 text input,
selection, clipboard actions, undo/redo, literal find, line navigation and
clickable compiler diagnostics. Ctrl+Space offers local ConcordScript keyword
and document-identifier suggestions. C++ `.cpp`, `.h`, and `.hpp` files inside
the project can be opened with the vendored C++ syntax colorizer; semantic
clangd integration is not yet available. Generated `SceneLayout.cx` and
`ProjectRuntime.cx` are read-only. New scripts must be imported or called by game
code to participate in its behavior.

## UI workspace

Open or create a `.yu` document. **UI Elements** contains the control hierarchy,
**UI Canvas** previews the interface, and **UI Properties** edits the selection.
Supported controls are Panel, Label, Button, Checkbox, Slider, Text input and
Progress.

Select a panel before adding children. Drag controls to position them and drag
their bottom-right corner to resize. Fit, zoom, grid and snapping controls are
available; holding Shift temporarily toggles snapping. Duplicate/delete actions
operate on a whole subtree. Undo/redo covers document edits. Properties include
stable IDs, parent, position, size, anchors, text and initial values, colors,
corner radius, font scale, visibility and enabled state. Saving a new document
defaults to `UI/Interface.yu` and creates its parent folders.

Buttons can minimize, maximize/restore, toggle fullscreen or close the game
window without script commands. A button with no built-in action still emits a
click event. Custom game behavior requires `.cx` code to consume `UiDocument`
events; the generated runtime currently draws the document and handles built-in
window actions, without adding custom event handlers.

The designer uses the engine's noninteractive preview mode, so clicking a
control selects it without triggering a window command or editing a runtime
value. **Play** runs the interactive interface. Layout scales uniformly to fit
the reference resolution; anchors position elements within their parent, and
children inherit clipping, visibility and enabled state. `.yu` supports up to
2,048 elements, 64 hierarchy levels and 4 MiB per document.

## Shortcuts, layout and recovery

| Shortcut | Action |
| --- | --- |
| Ctrl+S | Save project edits |
| F5 | Build and play |
| F11 | Toggle editor fullscreen |
| W / E / R | Move / rotate / scale in the 3D viewport |
| F / Delete | Focus / delete the selected 3D object |
| Ctrl+D | Duplicate the selected 3D object |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo in the active workspace |
| Ctrl+Y | Redo in the code editor |
| Ctrl+F / F3 | Find / next match in code |
| Ctrl+G | Go to source line |

Panels can be docked and resized. **View > Restore default layout** restores the
active workspace arrangement. Build output can be hidden when more working
space is needed.

Scripts and scenes save before switching files, with checks against changes
made outside the editor. Switching a dirty UI document prompts Save, Discard or
Cancel. UI documents use atomic saves and also reject external modifications.
On close, the editor makes a best-effort recovery copy of unsaved content:
`.editor/Recovery.scene`, `.editor/UI.recovery.yu`, or
`.editor/<script filename>.recovery`. Recovery is manual: copy the desired
backup to a project file and reopen it. There is no automatic restore dialog.

## Preview limits and implementation

The editor viewport and generated game runtime currently use the raster path.
Cloud, fog and postprocessing settings are serialized and exported, but are not
all represented by that path. A plane is a finite thin box. Model-import UI,
nested 3D scene hierarchies, animation editing, semantic language services and debugging
are not implemented. Play compiles and launches a separate game; the editor
does not execute arbitrary `.cx` behavior in its own viewport.

The UI designer is an anchored control system, not a CSS/flexbox layout engine.
It has no arbitrary image/SVG control. Window button actions are available, but
a `.yu` panel does not automatically become a draggable native title bar;
custom native drag regions are currently configured through the engine's C++
`Window::SetDragRegion` API.

Dear ImGui supplies widgets and docking, ImGuizmo supplies scene manipulators,
and ImGuiColorTextEdit supplies the source editor behind Concord's `CodeEditor`
API. Hello ImGui supplies theme tables and docking descriptions; its Runner and
alternative platform/rendering backends are not built. Concord owns the SDL3
window/input, frame loop and native Vulkan rendering, including the offscreen
scene viewport.

Logo and action icons are SVG sources in `assets/`, embedded during CMake
configuration. The engine's NanoSVG-based icon support handles monochrome solid
fills and strokes; gradients, masks and compound holes are outside its current
scope. Dependency revisions and licenses live in `../src/3rd/UI-VERSIONS.txt`
and the vendored library directories, and are copied into staged SDK notices.

With `CONCORD_BUILD_TESTS=ON`, editor test targets cover scene migration and
round trips (`concord_editor_document_test`), workspace layouts
(`concord_editor_layout_test`), safe project paths and settings
(`concord_editor_project_test`), startup/runtime generation
(`concord_editor_runtime_test`), and UI file switching, saves and recovery
(`concord_editor_ui_test`). Engine targets `concord_code_editor_tests`,
`concord_ui_toolkit_tests` and `concord_ui_document_tests` cover text-input
activation, editing, appearance, UI layout, serialization and input handling.
