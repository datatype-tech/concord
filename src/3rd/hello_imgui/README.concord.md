# Hello ImGui components used by Concord

Source: https://github.com/pthom/hello_imgui
Revision: `3135ade34abcbcfea0bf0dd8c8c4e84a1a350415`
License: MIT, retained in `LICENSE`.

`hello_imgui/imgui_theme.h` and `imgui_theme.cpp` provide the actual upstream
theme tables and color/rounding algorithms. The implementation omits its two
Runner `ThemeChanged` notifications, the two Runner includes and optional
ImGui demo/style-editor windows (which require `imgui_demo.cpp`). Concord
owns the context, frame lifecycle, window and appearance updates.

The upstream docking parameter headers are retained unchanged. The editor
uses those declarations and an attributed adaptation of the named split
application algorithm, with separate persisted layouts per workspace.

Hello ImGui's Runner, platform backends and renderer backends are not built.
SDL3 input and the native Concord Vulkan renderer consume the shared ImGui
context and draw data. No SDL2, GLFW or OpenGL dependency is introduced.
