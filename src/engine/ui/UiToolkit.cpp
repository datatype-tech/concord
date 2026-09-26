// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CUiToolkit.h"
#include "engine/window/Window.h"
#include "engine/window/WindowAccess.h"
#include <backends/imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <hello_imgui/imgui_theme.h>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace Concord {
struct UiToolkit::Impl {
    ImGuiContext* context = nullptr;
    Window* window = nullptr;
    u32 width = 960, height = 540;
    u64 texture = 0;
    bool visible = true;
    UiAppearance appearance{};
    float rasterScale = 2.0f, displayScale = 1.0f;
    bool appearanceDirty = true;
};
UiToolkit::UiToolkit() : m_impl(std::make_unique<Impl>()) {}
UiToolkit::~UiToolkit() { Shutdown(); }
void UiToolkit::Activate() const { ImGui::SetCurrentContext(m_impl->context); }
void UiToolkit::SetAppearance(const UiAppearance& appearance)
{
    m_impl->appearance = appearance;
    m_impl->appearance.theme = static_cast<UiToolkitTheme>(std::clamp(static_cast<int>(appearance.theme), 0, 3));
    m_impl->appearance.scale = std::isfinite(appearance.scale) ? std::clamp(appearance.scale, 0.85f, 1.6f) : 1.0f;
    m_impl->appearance.rounding = std::isfinite(appearance.rounding) ? std::clamp(appearance.rounding, 0.0f, 16.0f) : 8.0f;
    m_impl->appearanceDirty = true;
}
UiAppearance UiToolkit::Appearance() const noexcept { return m_impl->appearance; }
bool UiToolkit::Init(Window& window)
{
    Shutdown();
    m_impl->context = ImGui::CreateContext();
    Activate();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = nullptr;
    m_impl->displayScale = std::max(1.0f, SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(WindowAccess::NativeHandle(window))));
    m_impl->rasterScale = std::max(2.0f, m_impl->displayScale * 1.6f);
    const float raster = m_impl->rasterScale;
    /** Bake above the logical size so live UI scaling needs no GPU atlas rebuild. */
    const char* regular = std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf") ? "C:/Windows/Fonts/segoeui.ttf" : nullptr;
    ImFontConfig fallback;
    fallback.SizePixels = 18.0f * raster;
    ImFont* body = regular ? io.Fonts->AddFontFromFileTTF(regular, 18.0f * raster) : io.Fonts->AddFontDefault(&fallback);
    if (body && regular && std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
        ImFontConfig config;
        config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f * raster, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    const char* mono = std::filesystem::exists("C:/Windows/Fonts/consola.ttf") ? "C:/Windows/Fonts/consola.ttf" : regular;
    if (mono) io.Fonts->AddFontFromFileTTF(mono, 18.0f * raster);
    else io.Fonts->AddFontDefault(&fallback);
    if (mono && std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
        ImFontConfig config;
        config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f * raster, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    const char* heading = std::filesystem::exists("C:/Windows/Fonts/seguisb.ttf") ? "C:/Windows/Fonts/seguisb.ttf" : regular;
    fallback.SizePixels = 26.0f * raster;
    if (heading) io.Fonts->AddFontFromFileTTF(heading, 26.0f * raster);
    else io.Fonts->AddFontDefault(&fallback);
    if (std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
        ImFontConfig config;
        config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 26.0f * raster, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    }
    m_impl->appearanceDirty = true;
    if (!ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(WindowAccess::NativeHandle(window)))) {
        ImGui::DestroyContext(m_impl->context); m_impl->context = nullptr; return false;
    }
    m_impl->window = &window;
    WindowAccess::SetEventObserver(window, [this](const void* event) {
        Activate(); ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(event));
    });
    return true;
}
void UiToolkit::Shutdown()
{
    if (!m_impl->context) return;
    Activate();
    if (m_impl->window) {
        WindowAccess::SetEventObserver(*m_impl->window, {});
        ImGui_ImplSDL3_Shutdown(); m_impl->window = nullptr;
    }
    ImGui::DestroyContext(m_impl->context); m_impl->context = nullptr;
    m_impl->texture = 0;
}
void UiToolkit::Begin()
{
    Activate();
    ImGui_ImplSDL3_NewFrame();
    const float displayScale = std::max(1.0f, SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(WindowAccess::NativeHandle(*m_impl->window))));
    if (m_impl->appearanceDirty || displayScale != m_impl->displayScale) {
        m_impl->displayScale = displayScale;
        const ImGuiTheme::ImGuiTheme_ themes[] = {
            ImGuiTheme::ImGuiTheme_SoDark_AccentBlue, ImGuiTheme::ImGuiTheme_DarculaDarker,
            ImGuiTheme::ImGuiTheme_PhotoshopStyle, ImGuiTheme::ImGuiTheme_GrayVariations_Darker
        };
        ImGuiTheme::ImGuiTweakedTheme theme(themes[static_cast<int>(m_impl->appearance.theme)]);
        theme.Tweaks.Rounding = m_impl->appearance.rounding;
        auto style = ImGuiTheme::TweakedThemeThemeToStyle(theme);
        style.WindowPadding = {10, 8};
        style.FramePadding = {8, 4};
        style.ItemSpacing = {7, 6};
        style.WindowBorderSize = 0;
        style.ChildBorderSize = 1;
        style.ScrollbarSize = 12;
        style.WindowRounding = m_impl->appearance.rounding;
        style.ChildRounding = m_impl->appearance.rounding * 0.7f;
        style.FrameRounding = m_impl->appearance.rounding * 0.65f;
        style.PopupRounding = m_impl->appearance.rounding;
        style.TabRounding = m_impl->appearance.rounding * 0.65f;
        style.GrabRounding = m_impl->appearance.rounding * 0.5f;
        const float scale = displayScale * m_impl->appearance.scale;
        style.ScaleAllSizes(scale);
        ImGui::GetStyle() = style;
        ImGui::GetIO().FontGlobalScale = scale / m_impl->rasterScale;
        m_impl->appearanceDirty = false;
    }
    ImGui::NewFrame();
    m_impl->visible = false;
}
void UiToolkit::End() { Activate(); ImGui::Render(); }
void UiToolkit::SetSceneViewport(u32 width, u32 height) noexcept
{
    m_impl->width = std::clamp(width, 32u, 4096u);
    m_impl->height = std::clamp(height, 32u, 4096u);
    m_impl->visible = width > 0 && height > 0;
}
ImTextureID UiToolkit::SceneTexture() const noexcept { return m_impl->texture; }
u32 UiToolkit::ViewportWidth() const noexcept { return m_impl->width; }
u32 UiToolkit::ViewportHeight() const noexcept { return m_impl->height; }
bool UiToolkit::SceneVisible() const noexcept { return m_impl->visible; }
void UiToolkit::SetSceneTexture(u64 texture) noexcept { m_impl->texture = texture; }
void* UiToolkit::DrawData() const noexcept { Activate(); return ImGui::GetDrawData(); }
}
