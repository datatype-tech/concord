// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include <Concord/CUiDocument.h>
#include <Concord/CWindow.h>

#include <imgui.h>

#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace Concord;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void MustReject(const UiDocument& document)
{
    bool rejected = false;
    try { document.Validate(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "invalid authored UI document was accepted");
}

UiDocument Fixture()
{
    UiDocument document;
    document.referenceSize = {640, 360};
    document.elements = {
        {.id="button", .text="Close \"window\"", .kind=UiElementKind::Button, .position={20,20}, .size={160,48}, .action=UiAction::CloseWindow},
        {.id="check", .text="Enabled", .kind=UiElementKind::Checkbox, .position={20,90}, .size={200,40}},
        {.id="slider", .text="Volume", .kind=UiElementKind::Slider, .position={20,150}, .size={200,40}, .value=0.25f},
        {.id="input", .text="Name", .kind=UiElementKind::TextInput, .position={20,210}, .size={200,40}},
        {.id="progress", .text="Loading", .kind=UiElementKind::Progress, .position={300,20}, .size={200,40}, .value=0.4f},
        {.id="panel", .text="", .kind=UiElementKind::Panel, .position={300,100}, .size={240,150}},
        {.id="label", .parent="panel", .text="UTF-8: \xe4\xb8\xad\nSecond line", .kind=UiElementKind::Label, .position={10,10}, .size={200,50}}
    };
    return document;
}

void CheckSerialization()
{
    const auto original = Fixture();
    const auto text = original.Serialize();
    UiDocument parsed;
    parsed.Parse(text);
    Require(parsed.Serialize() == text, "UI round trip lost values, hierarchy or quoted UTF-8 text");
    for (const std::string& invalid : {
        std::string("CONCORD_UI 9\n640 360 0\n"), std::string("CONCORD_UI 1\n640 360 999999\n"),
        text + "unexpected", text.substr(0, text.size() / 2), std::string(4 * 1024 * 1024 + 1, 'x')}) {
        bool rejected = false;
        try { parsed.Parse(invalid); } catch (const std::exception&) { rejected = true; }
        Require(rejected && parsed.Serialize() == text, "failed .yu parse replaced live document");
    }
    auto invalid = original;
    invalid.elements[1].id = invalid.elements[0].id; MustReject(invalid);
    invalid = original; invalid.elements[0].parent = "missing"; MustReject(invalid);
    invalid = original; invalid.elements[0].parent = "input"; MustReject(invalid);
    invalid = original; invalid.elements[5].parent = "panel"; MustReject(invalid);
    invalid = original; invalid.elements[6].kind = UiElementKind::Panel; invalid.elements[5].parent = "label"; MustReject(invalid);
    invalid = original; invalid.elements[0].size.x = 0; MustReject(invalid);
    invalid = original; invalid.elements[0].anchor.x = 1.1f; MustReject(invalid);
    invalid = original; invalid.elements[0].position.x = std::numeric_limits<float>::infinity(); MustReject(invalid);
    invalid = original; invalid.elements[1].action = UiAction::CloseWindow; MustReject(invalid);
    invalid = original; invalid.elements[3].input = std::string("a\0b", 3); MustReject(invalid);
    invalid = original; invalid.elements[1].kind = static_cast<UiElementKind>(42); MustReject(invalid);
    invalid = original; invalid.elements.clear();
    for (int index = 0; index < 65; ++index)
        invalid.elements.push_back({.id="panel" + std::to_string(index), .parent=index ? "panel" + std::to_string(index-1) : "", .kind=UiElementKind::Panel});
    MustReject(invalid);

    const auto path = std::filesystem::temp_directory_path() / ("concord-ui-test-" + std::to_string(reinterpret_cast<std::uintptr_t>(&parsed)) + ".yu");
    original.Save(path);
    parsed.Load(path);
    Require(parsed.Serialize() == text, "saved .yu document differs from its source");
    parsed.elements[1].value = 1;
    parsed.Save(path);
    UiDocument replaced; replaced.Load(path);
    Require(replaced.elements[1].value == 1, "atomic save failed to replace existing document");
    auto invalidSave = replaced;
    invalidSave.elements[0].parent = "missing";
    bool saveRejected = false;
    try { invalidSave.Save(path); } catch (const std::exception&) { saveRejected = true; }
    UiDocument preserved; preserved.Load(path);
    Require(saveRejected && preserved.Serialize() == replaced.Serialize(), "invalid save overwrote the previous file");
    std::filesystem::remove(path);
}

void CheckLayout()
{
    UiDocument document;
    document.referenceSize = {800, 600};
    document.elements = {
        {.id="child", .parent="parent", .kind=UiElementKind::Button, .position={-10,-10}, .size={100,40}, .anchor={1,1}},
        {.id="parent", .text="", .kind=UiElementKind::Panel, .position={-20,-20}, .size={300,200}, .anchor={1,1}}
    };
    auto result = document.ResolveLayout({.position={10,20}, .size={1600,600}});
    Require(result.size() == 2 && result[0].index == 0, "layout lost document indices");
    Require(result[1].position.x == 1290 && result[1].position.y == 400, "parent did not anchor to resized viewport");
    Require(result[0].position.x == 1480 && result[0].position.y == 550 && result[0].size.x == 100,
        "forward parent reference or nested anchor resolved incorrectly");
    document.elements[0].position = {100,0};
    result = document.ResolveLayout({.size={800,600}});
    Require(!result[0].visible && result[0].clipSize.x == 0, "child outside panel was not clipped");
    document.elements[0].position = {0,0};
    document.elements[1].visible = false;
    document.elements[1].enabled = false;
    result = document.ResolveLayout({.size={800,600}});
    Require(!result[0].visible && !result[0].enabled, "child did not inherit visibility and enabled state");
    document.elements[1].visible = true;
    result = document.ResolveLayout({.size={400,300}});
    Require(result[0].size.x == 50 && result[1].size.y == 100, "uniform preview scaling changed authored dimensions");
}

class FrameFixture {
public:
    FrameFixture()
    {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {640,360};
        io.ConfigInputTrickleEventQueue = false;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(1);
    }
    ~FrameFixture() { ImGui::DestroyContext(); }

    std::vector<UiEvent> Frame(UiDocument& document, bool interactive = true, Window* window = nullptr,
                               const std::function<void()>& input = {})
    {
        if (input) input();
        ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({640,360});
        ImGui::Begin("UI document tests", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        auto events = document.Draw({.position={0,0}, .size={640,360}, .interactive=interactive, .window=window});
        ImGui::End();
        ImGui::Render();
        return events;
    }

    std::vector<UiEvent> Click(UiDocument& document, float x, float y, bool interactive = true, Window* window = nullptr)
    {
        auto events = Frame(document, interactive, window, [&] {
            ImGui::GetIO().AddMousePosEvent(x,y);
            ImGui::GetIO().AddMouseButtonEvent(0,true);
        });
        auto released = Frame(document, interactive, window, [] { ImGui::GetIO().AddMouseButtonEvent(0,false); });
        events.insert(events.end(), released.begin(), released.end());
        return events;
    }
};

void CheckInteraction()
{
    auto document = Fixture();
    FrameFixture fixture;
    fixture.Frame(document);
    /** Finish ImGui's initial keyboard-navigation focus before delivering mouse input. */
    fixture.Frame(document);
    Window window;
    auto events = fixture.Click(document,50,40,true,&window);
    Require(events.size() == 1 && events[0].id == "button" && events[0].kind == UiEventKind::Clicked && window.ShouldClose(),
        "button event did not execute its engine window action");
    document.elements[0].action = UiAction::ToggleFullscreenWindow;
    fixture.Click(document,50,40,true,&window);
    Require(window.Mode() == WindowMode::Fullscreen, "fullscreen action did not reach the engine window");
    fixture.Click(document,50,40,true,&window);
    Require(window.Mode() == WindowMode::Windowed, "fullscreen action failed to restore the prior window mode");
    events = fixture.Click(document,50,110);
    Require(events.size() == 1 && events[0].kind == UiEventKind::Changed && document.elements[1].value == 1,
        "checkbox did not update the document and report its value");
    events = fixture.Click(document,190,170);
    Require(!events.empty() && events[0].id == "slider" && document.elements[2].value > 0.7f,
        "slider did not update through ImGui input");
    fixture.Click(document,60,230);
    events = fixture.Frame(document, true, nullptr, [] { ImGui::GetIO().AddInputCharactersUTF8("hello"); });
    Require(document.elements[3].input == "hello" && events.size() == 1 && events[0].input == "hello",
        "text input did not publish changed text");
    document.elements[1].enabled = false;
    events = fixture.Click(document,50,110);
    Require(events.empty() && document.elements[1].value == 1, "disabled control consumed input");
    document.elements[1].enabled = true;
    events = fixture.Click(document,50,110,false);
    Require(events.empty() && document.elements[1].value == 1, "designer preview changed live control values");
    document.elements[1].visible = false;
    events = fixture.Click(document,50,110);
    Require(events.empty() && document.elements[1].value == 1, "hidden control consumed input");
    Require(ImGui::GetDrawData()->TotalVtxCount > 0, "document emitted no render geometry");
}
}

int main()
{
    try {
        CheckSerialization();
        CheckLayout();
        CheckInteraction();
        std::cout << "UiDocument: parser, atomic save, hierarchy, anchor layout, clipping, native widgets and window actions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "UiDocument regression: " << error.what() << '\n';
        return 1;
    }
}
