// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include <Concord/CApplication.h>
#include <Concord/CUiToolkit.h>

#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool Near(float a, float b, float tolerance = 0.02f)
{
    return std::abs(a - b) <= tolerance;
}

struct Metrics {
    float fontSize = 0;
    float frameHeight = 0;
    float atlasFontSize = 0;
};

/** Uses the real platform and theme implementation without creating a GPU device. */
class ToolkitFixture {
public:
    ToolkitFixture()
        : m_window({.title = "Native UI appearance regression", .resolution = {640, 480}, .visible = false}),
          m_game({.enableRendering = false})
    {
        m_game.AttachWindow(m_window);
        Require(m_window.IsOpen(), "hidden native window did not open");
        Require(ui.Init(m_window), "native UI initialization failed");
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        Require(pixels && width > 0 && height > 0, "native font atlas did not build");
        io.Fonts->SetTexID(1);
    }

    ~ToolkitFixture()
    {
        ui.Shutdown();
        m_game.DetachWindow();
    }

    Metrics Frame(const std::function<void()>& content = {})
    {
        ui.Begin();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({600, 440});
        ImGui::Begin("Appearance", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        Metrics metrics{ImGui::GetFontSize(), ImGui::GetFrameHeight(), ImGui::GetFont()->FontSize};
        ImGui::TextUnformatted("Concord native UI");
        ImGui::Button("Control height");
        if (content) content();
        ImGui::End();
        ui.End();
        return metrics;
    }

private:
    Concord::Window m_window;
    Concord::Game m_game;

public:
    Concord::UiToolkit ui;
};

void CheckFrameBoundaryAndScaling(ToolkitFixture& fixture)
{
    fixture.ui.SetAppearance({.theme = Concord::UiToolkitTheme::SoDark, .scale = 1.0f});
    const Metrics baseline = fixture.Frame();
    Require(baseline.fontSize > 0, "default UI font has no size");

    fixture.Frame([&] {
        const float before = ImGui::GetIO().FontGlobalScale;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {101, 102});
        fixture.ui.SetAppearance({.theme = Concord::UiToolkitTheme::Darcula, .scale = 1.25f});
        Require(Near(ImGui::GetIO().FontGlobalScale, before), "appearance request changed fonts in the middle of a frame");
        Require(Near(ImGui::GetStyle().FramePadding.y, 102), "appearance request overwrote a pushed style");
        ImGui::PopStyleVar();
    });

    const Metrics scaled = fixture.Frame();
    Require(Near(scaled.fontSize, baseline.fontSize * 1.25f), "UI font scale was not applied exactly once");
    Require(Near(scaled.atlasFontSize, baseline.atlasFontSize), "changing UI scale rebuilt font raster sizes");
    Require(scaled.frameHeight > baseline.frameHeight, "controls did not grow with the font");
    for (int index = 0; index < 24; ++index) {
        fixture.ui.SetAppearance({.theme = static_cast<Concord::UiToolkitTheme>(index % 4), .scale = 1.25f});
        const Metrics current = fixture.Frame();
        Require(Near(current.fontSize, scaled.fontSize), "repeated theme changes accumulated font scaling");
        Require(Near(current.frameHeight, scaled.frameHeight), "repeated theme changes accumulated control scaling");
    }
    fixture.ui.SetAppearance({.theme = Concord::UiToolkitTheme::SoDark, .scale = 1.0f});
    const Metrics restored = fixture.Frame();
    Require(Near(restored.fontSize, baseline.fontSize) && Near(restored.frameHeight, baseline.frameHeight),
            "returning to the original scale did not restore font and control dimensions");
}

void CheckFontRolesAndGlyphs(ToolkitFixture& fixture)
{
    fixture.Frame([] {
        auto& fonts = ImGui::GetIO().Fonts->Fonts;
        Require(fonts.Size >= 3, "body, code, and heading fonts were not created");
        if (std::filesystem::exists("C:/Windows/Fonts/consola.ttf")) {
            ImGui::PushFont(fonts[1]);
            Require(Near(ImGui::CalcTextSize("iiii").x, ImGui::CalcTextSize("WWWW").x),
                    "code editor font is not monospaced");
            ImGui::PopFont();
        }
        if (std::filesystem::exists("C:/Windows/Fonts/msyh.ttc")) {
            for (int index = 0; index < 2; ++index) {
                Require(fonts[index]->FindGlyphNoFallback(0x4e2d) != nullptr &&
                        fonts[index]->FindGlyphNoFallback(0x6587) != nullptr,
                        "body or code font lacks common CJK glyphs");
            }
        }
        if (std::filesystem::exists("C:/Windows/Fonts/seguisb.ttf"))
            Require(fonts[2]->FontSize > fonts[0]->FontSize, "heading font lost its size hierarchy");
    });
}

void CheckInvalidPreferences(ToolkitFixture& fixture)
{
    fixture.ui.SetAppearance({.theme = static_cast<Concord::UiToolkitTheme>(-8),
                              .scale = std::numeric_limits<float>::quiet_NaN()});
    auto appearance = fixture.ui.Appearance();
    Require(appearance.theme == Concord::UiToolkitTheme::SoDark && appearance.scale == 1.0f,
            "invalid preferences were not normalized");
    fixture.Frame();
    fixture.ui.SetAppearance({.theme = static_cast<Concord::UiToolkitTheme>(100), .scale = 100.0f});
    appearance = fixture.ui.Appearance();
    Require(appearance.theme == Concord::UiToolkitTheme::Gray && appearance.scale == 1.6f,
            "out-of-range preferences were not clamped");
    fixture.Frame();
}

} // namespace

int main()
{
    try {
        ToolkitFixture fixture;
        CheckFrameBoundaryAndScaling(fixture);
        CheckFontRolesAndGlyphs(fixture);
        CheckInvalidPreferences(fixture);
        std::cout << "Native UI appearance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native UI appearance regression: " << error.what() << '\n';
        return 1;
    }
}
