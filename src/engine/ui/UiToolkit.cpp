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
#include <unordered_map>
#include <utility>

namespace Concord {
namespace {
/** Godot-style surfaces: blue-gray panels, inset fields, raised buttons, a quiet accent. */
void ApplyConcordColors(ImGuiStyle& style)
{
    const ImVec4 accent{0.44f, 0.73f, 0.98f, 1.0f};
    const ImVec4 panel{0.210f, 0.240f, 0.290f, 1.0f};
    const ImVec4 chrome{0.147f, 0.168f, 0.203f, 1.0f};
    const ImVec4 well{0.118f, 0.135f, 0.165f, 1.0f};
    const ImVec4 raised{0.275f, 0.310f, 0.365f, 1.0f};
    const ImVec4 raisedHover{0.330f, 0.368f, 0.428f, 1.0f};
    const ImVec4 selection{0.275f, 0.455f, 0.655f, 1.0f};
    const ImVec4 selectionHover{0.330f, 0.520f, 0.730f, 1.0f};
    auto& c = style.Colors;
    c[ImGuiCol_Text] = {0.925f, 0.940f, 0.960f, 1.0f};
    c[ImGuiCol_TextDisabled] = {0.580f, 0.620f, 0.680f, 1.0f};
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_PopupBg] = {0.125f, 0.145f, 0.180f, 0.98f};
    c[ImGuiCol_Border] = {0.520f, 0.570f, 0.640f, 0.45f};
    c[ImGuiCol_BorderShadow] = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_FrameBg] = well;
    c[ImGuiCol_FrameBgHovered] = {0.155f, 0.175f, 0.210f, 1.0f};
    c[ImGuiCol_FrameBgActive] = {0.100f, 0.115f, 0.145f, 1.0f};
    c[ImGuiCol_TitleBg] = chrome;
    c[ImGuiCol_TitleBgActive] = chrome;
    c[ImGuiCol_TitleBgCollapsed] = chrome;
    c[ImGuiCol_MenuBarBg] = chrome;
    c[ImGuiCol_ScrollbarBg] = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_ScrollbarGrab] = {0.780f, 0.830f, 0.900f, 0.22f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.780f, 0.830f, 0.900f, 0.36f};
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = {0.62f, 0.84f, 1.0f, 1.0f};
    c[ImGuiCol_Button] = raised;
    c[ImGuiCol_ButtonHovered] = raisedHover;
    c[ImGuiCol_ButtonActive] = {0.210f, 0.240f, 0.290f, 1.0f};
    c[ImGuiCol_Header] = selection;
    c[ImGuiCol_HeaderHovered] = selectionHover;
    c[ImGuiCol_HeaderActive] = {0.220f, 0.380f, 0.560f, 1.0f};
    c[ImGuiCol_Separator] = {0.0f, 0.0f, 0.0f, 0.38f};
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_ResizeGripHovered] = selectionHover;
    c[ImGuiCol_ResizeGripActive] = accent;
    c[ImGuiCol_Tab] = chrome;
    c[ImGuiCol_TabHovered] = raised;
    c[ImGuiCol_TabSelected] = panel;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = chrome;
    c[ImGuiCol_TabDimmedSelected] = {0.185f, 0.210f, 0.255f, 1.0f};
    c[ImGuiCol_TabDimmedSelectedOverline] = {1.0f, 1.0f, 1.0f, 0.22f};
    c[ImGuiCol_DockingPreview] = {accent.x, accent.y, accent.z, 0.35f};
    c[ImGuiCol_DockingEmptyBg] = {0.090f, 0.105f, 0.130f, 1.0f};
    c[ImGuiCol_PlotLines] = accent;
    c[ImGuiCol_PlotLinesHovered] = c[ImGuiCol_SliderGrabActive];
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_PlotHistogramHovered] = c[ImGuiCol_SliderGrabActive];
    c[ImGuiCol_TableHeaderBg] = chrome;
    c[ImGuiCol_TableBorderStrong] = {0.0f, 0.0f, 0.0f, 0.40f};
    c[ImGuiCol_TableBorderLight] = {1.0f, 1.0f, 1.0f, 0.06f};
    c[ImGuiCol_TableRowBg] = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TableRowBgAlt] = {1.0f, 1.0f, 1.0f, 0.035f};
    c[ImGuiCol_TextLink] = accent;
    c[ImGuiCol_TextSelectedBg] = {selection.x, selection.y, selection.z, 0.55f};
    c[ImGuiCol_DragDropTarget] = accent;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_NavWindowingHighlight] = {1.0f, 1.0f, 1.0f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg] = {0.0f, 0.0f, 0.0f, 0.45f};
    c[ImGuiCol_ModalWindowDimBg] = {0.02f, 0.03f, 0.05f, 0.55f};
}
}

struct UiToolkit::Impl {
    ImGuiContext* context = nullptr;
    Window* window = nullptr;
    u32 width = 960, height = 540;
    u64 texture = 0;
    bool visible = true;
    UiAppearance appearance{};
    float rasterScale = 2.0f, displayScale = 1.0f;
    bool appearanceDirty = true;
    u32 nextImage = 1;
    std::vector<UiImagePixels> pendingImages;
    std::unordered_map<u32, u64> imageTextures;
    /** Referenced by the font atlas until it is built, so it lives with the context. */
    ImVector<ImWchar> chineseRanges;
};
UiToolkit::UiToolkit() : m_impl(std::make_unique<Impl>()) {}
UiToolkit::~UiToolkit() { Shutdown(); }
void UiToolkit::Activate() const { ImGui::SetCurrentContext(m_impl->context); }
void UiToolkit::SetAppearance(const UiAppearance& appearance)
{
    m_impl->appearance = appearance;
    m_impl->appearance.theme = static_cast<UiToolkitTheme>(std::clamp(static_cast<int>(appearance.theme), 0, 3));
    m_impl->appearance.scale = std::isfinite(appearance.scale) ? std::clamp(appearance.scale, 0.85f, 1.6f) : 1.0f;
    m_impl->appearance.rounding = std::isfinite(appearance.rounding) ? std::clamp(appearance.rounding, 0.0f, 16.0f) : 0.0f;
    m_impl->appearanceDirty = true;
}
UiAppearance UiToolkit::Appearance() const noexcept { return m_impl->appearance; }
bool UiToolkit::Init(Window& window, const char* extraGlyphs)
{
    Shutdown();
    m_impl->context = ImGui::CreateContext();
    Activate();
    ImGuiIO& io = ImGui::GetIO();
    {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (extraGlyphs) builder.AddText(extraGlyphs);
        m_impl->chineseRanges.clear();
        builder.BuildRanges(&m_impl->chineseRanges);
    }
    const ImWchar* chinese = m_impl->chineseRanges.Data;
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
        config.GlyphOffset.y = 1.0f;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f * raster, &config, chinese);
    }
    const char* mono = std::filesystem::exists("C:/Windows/Fonts/consola.ttf") ? "C:/Windows/Fonts/consola.ttf" : regular;
    if (mono) io.Fonts->AddFontFromFileTTF(mono, 18.0f * raster);
    else io.Fonts->AddFontDefault(&fallback);
    if (mono && std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
        ImFontConfig config;
        config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f * raster, &config, chinese);
    }
    const char* heading = std::filesystem::exists("C:/Windows/Fonts/seguisb.ttf") ? "C:/Windows/Fonts/seguisb.ttf" : regular;
    fallback.SizePixels = 26.0f * raster;
    if (heading) io.Fonts->AddFontFromFileTTF(heading, 26.0f * raster);
    else io.Fonts->AddFontDefault(&fallback);
    if (std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
        ImFontConfig config;
        config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 26.0f * raster, &config, chinese);
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
    m_impl->imageTextures.clear();
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
        const float rounding = m_impl->appearance.rounding;
        style.WindowPadding = {10, 8};
        style.FramePadding = {8, 5};
        style.ItemSpacing = {8, 6};
        style.ItemInnerSpacing = {6, 4};
        style.CellPadding = {8, 5};
        style.IndentSpacing = 16;
        style.WindowMinSize = {180, 100};
        style.WindowBorderSize = 0;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 0;
        style.TabBorderSize = 0;
        style.ScrollbarSize = 11;
        style.GrabMinSize = 10;
        style.WindowRounding = rounding;
        style.ChildRounding = rounding * 0.7f;
        style.FrameRounding = rounding * 0.6f;
        style.PopupRounding = rounding * 0.75f;
        style.TabRounding = rounding * 0.6f;
        style.GrabRounding = rounding * 0.5f;
        style.ScrollbarRounding = rounding;
        style.TabBarBorderSize = 1;
        style.TabBarOverlineSize = 2;
        style.SeparatorTextBorderSize = 1;
        style.SeparatorTextPadding = {0, 3};
        style.DockingSeparatorSize = 1;
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.SelectableTextAlign = {0.0f, 0.5f};
        if (m_impl->appearance.theme == UiToolkitTheme::Darcula) {
            ApplyConcordColors(style);
            const float radius = std::clamp(rounding, 0.0f, 8.0f);
            style.WindowPadding = {6, 6};
            style.FramePadding = {7, 4};
            style.ItemSpacing = {6, 4};
            style.ItemInnerSpacing = {5, 4};
            style.CellPadding = {6, 3};
            style.WindowRounding = 0;
            style.ChildRounding = 0;
            style.PopupRounding = radius;
            style.FrameRounding = radius;
            style.GrabRounding = radius;
            style.TabRounding = radius;
            style.ScrollbarRounding = std::max(radius, 6.0f);
            style.FrameBorderSize = 1;
            style.PopupBorderSize = 1;
            style.ChildBorderSize = 0;
            style.WindowBorderSize = 0;
            style.ScrollbarSize = 9;
            style.GrabMinSize = 12;
            style.DockingSeparatorSize = 2;
            style.TabBarOverlineSize = 2;
            style.TabBarBorderSize = 0;
        }
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
u32 UiToolkit::AddImage(const u8* rgba, u32 width, u32 height)
{
    if (!rgba || width == 0 || height == 0 || width > 4096 || height > 4096) return 0;
    UiImagePixels image{.id = m_impl->nextImage++, .width = width, .height = height};
    image.rgba.assign(rgba, rgba + static_cast<usize>(width) * height * 4);
    const u32 id = image.id;
    m_impl->pendingImages.push_back(std::move(image));
    return id;
}
ImTextureID UiToolkit::ImageTexture(u32 image) const noexcept
{
    const auto found = m_impl->imageTextures.find(image);
    return found == m_impl->imageTextures.end() ? ImTextureID{} : static_cast<ImTextureID>(found->second);
}
std::vector<UiImagePixels> UiToolkit::TakePendingImages() { return std::exchange(m_impl->pendingImages, {}); }
void UiToolkit::SetImageTexture(u32 image, u64 texture) noexcept
{
    try { m_impl->imageTextures[image] = texture; } catch (...) {}
}
void* UiToolkit::DrawData() const noexcept { Activate(); return ImGui::GetDrawData(); }
}
