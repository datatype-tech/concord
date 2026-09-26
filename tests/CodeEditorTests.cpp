// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include <Concord/CCodeEditor.h>

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui_internal.h>

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string clipboard;

void Require(bool condition,const char* message)
{
    if(!condition)throw std::runtime_error(message);
}

const char* GetClipboard(ImGuiContext*) { return clipboard.c_str(); }
void SetClipboard(ImGuiContext*,const char* text) { clipboard=text; }

/** Exercises the shipping platform backend in a hidden native window, without a GPU. */
class EditorFixture {
public:
    EditorFixture()
    {
        Require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        m_window=SDL_CreateWindow("CodeEditor input regression",800,600,SDL_WINDOW_HIDDEN);
        Require(m_window!=nullptr,SDL_GetError());
        ImGui::CreateContext();
        auto& io=ImGui::GetIO();
        io.IniFilename=nullptr;
        io.ConfigInputTrickleEventQueue=false;
        io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_NavEnableKeyboard;
        Require(ImGui_ImplSDL3_InitForOther(m_window),"SDL backend initialization failed");
        ImGui::GetPlatformIO().Platform_GetClipboardTextFn=GetClipboard;
        ImGui::GetPlatformIO().Platform_SetClipboardTextFn=SetClipboard;
        unsigned char* pixels=nullptr;
        int width=0,height=0;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        io.Fonts->SetTexID(1);
    }
    ~EditorFixture()
    {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    bool Frame(const std::function<void()>& input={},bool focusOther=false)
    {
        ImGui_ImplSDL3_NewFrame();
        if(input)input();
        ImGui::GetIO().DeltaTime=1.0f/60.0f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({800,600});
        ImGui::Begin("Host",nullptr,ImGuiWindowFlags_NoDecoration);
        if(focusOther)ImGui::SetKeyboardFocusHere();
        ImGui::InputText("Other field",m_other,sizeof(m_other));
        const bool changed=editor.Render("Source");
        m_editorMin=ImGui::GetItemRectMin();
        ImGui::End();
        ImGui::Render();
        return changed;
    }

    void FocusEditor()
    {
        Frame([&] {
            auto& io=ImGui::GetIO();
            io.AddMousePosEvent(m_editorMin.x+50,m_editorMin.y+8);
            io.AddMouseButtonEvent(0,true);
        });
        Frame([] { ImGui::GetIO().AddMouseButtonEvent(0,false); });
    }

    bool Text(const char* text)
    {
        return Frame([&] {
            SDL_Event event{};
            event.type=SDL_EVENT_TEXT_INPUT;
            event.text.windowID=SDL_GetWindowID(m_window);
            event.text.text=text;
            Require(ImGui_ImplSDL3_ProcessEvent(&event),"text event was rejected");
        });
    }

    void Key(ImGuiKey key,bool control=false,bool shift=false)
    {
        Frame([&] {
            auto& io=ImGui::GetIO();
            if(control)io.AddKeyEvent(ImGuiMod_Ctrl,true);
            if(shift)io.AddKeyEvent(ImGuiMod_Shift,true);
            io.AddKeyEvent(key,true);
        });
        Frame([&] {
            auto& io=ImGui::GetIO();
            io.AddKeyEvent(key,false);
            if(control)io.AddKeyEvent(ImGuiMod_Ctrl,false);
            if(shift)io.AddKeyEvent(ImGuiMod_Shift,false);
        });
    }

    bool TextInputActive() const { return SDL_TextInputActive(m_window); }
    std::string OtherText() const { return m_other; }
    Concord::CodeEditor editor;

private:
    SDL_Window* m_window=nullptr;
    ImVec2 m_editorMin{};
    char m_other[64]{};
};

void CheckNativeInputAndFocus(EditorFixture& fixture)
{
    fixture.editor.SetText("");
    fixture.Frame();
    fixture.FocusEditor();
    Require(fixture.editor.IsFocused(),"click did not focus the code editor");
    Require(fixture.TextInputActive(),"focusing editor did not start SDL text input");
    Require(fixture.Text("abc"),"typing was not reported as a change");
    Require(fixture.editor.GetText()=="abc","ASCII input did not reach document");
    const ImVec2 firstCaret=ImGui::GetCurrentContext()->PlatformImeData.InputPos;
    fixture.Key(ImGuiKey_Enter);
    fixture.Key(ImGuiKey_Tab);
    fixture.Text("\xe4\xb8\xad");
    Require(fixture.editor.GetText().find("\xe4\xb8\xad")!=std::string::npos,"UTF-8 input was dropped");
    const auto caret=ImGui::GetCurrentContext()->PlatformImeData.InputPos;
    Require(caret.y>firstCaret.y && caret.x>firstCaret.x,"IME candidate anchor did not follow caret");
    fixture.Key(ImGuiKey_Backspace);
    Require(fixture.editor.GetText().find("\xe4\xb8\xad")==std::string::npos,"backspace corrupted UTF-8");
    const std::string before=fixture.editor.GetText();
    fixture.Key(ImGuiKey_Z,true);
    Require(fixture.editor.GetText()!=before,"keyboard undo did not restore deletion");
    fixture.Key(ImGuiKey_Y,true);
    Require(fixture.editor.GetText()==before,"keyboard redo failed");
    fixture.Key(ImGuiKey_Z,true);
    fixture.Key(ImGuiKey_Z,true,true);
    Require(fixture.editor.GetText()==before,"Ctrl+Shift+Z redo failed");

    fixture.Frame({},true);
    fixture.Frame();
    fixture.Text("outside");
    Require(fixture.OtherText()=="outside","another text field did not receive focus");
    Require(fixture.editor.GetText()==before,"editor consumed input from another field");

    fixture.FocusEditor();
    fixture.editor.SetReadOnly(true);
    fixture.Frame();
    Require(!fixture.TextInputActive(),"read-only editor kept native text input active");
    fixture.Text("forbidden");
    fixture.editor.Undo();
    fixture.editor.Cut();
    clipboard="forbidden";
    fixture.editor.Paste();
    Require(fixture.editor.GetText()==before,"read-only editor changed");
    Require(!fixture.Frame(),"read-only operations marked document dirty");
    fixture.editor.SetReadOnly(false);
    fixture.Frame();
    Require(fixture.TextInputActive(),"editable mode did not restart SDL text input");
    fixture.Frame([] { ImGui::GetIO().AddFocusEvent(false); });
    Require(!fixture.TextInputActive(),"application focus loss left text input active");
    fixture.Frame([] { ImGui::GetIO().AddFocusEvent(true); });
    Require(fixture.TextInputActive(),"application focus return did not restore text input");
}

void CheckDocumentActions(EditorFixture& fixture)
{
    for(const char* source : {"","one line","with newline\n","two empty lines\n\n","Windows\r\nnewlines\r\n","mixed\r\nnewlines\n"}) {
        fixture.editor.SetText(source);
        Require(fixture.editor.GetText()==source,"document load added or removed a final newline");
    }
    fixture.editor.SetText("first\r\nsecond\r\n");
    Require(fixture.editor.FindNext("second") && fixture.editor.CursorLine()==2 &&
        fixture.editor.CursorColumn()==1,"find counted CRLF as two characters");
    fixture.editor.GoToLine(2);
    clipboard="prefix";
    fixture.editor.Paste();
    Require(fixture.editor.GetText()=="first\r\nprefixsecond\r\n","editing changed Windows line endings");
    fixture.editor.Undo();
    Require(fixture.editor.GetText()=="first\r\nsecond\r\n","undo did not recover original source bytes");
    fixture.editor.SetText("alpha\n\tbeta alpha");
    Require(fixture.editor.FindNext("alpha"),"find missed first match");
    Require(fixture.editor.CursorLine()==1,"first match has wrong line");
    Require(fixture.editor.FindNext("alpha"),"find missed second match");
    Require(fixture.editor.CursorLine()==2 && fixture.editor.CursorColumn()==10,"tab-aware match coordinate is wrong");
    fixture.editor.Copy();
    Require(clipboard=="alpha","copy did not preserve selected text");
    fixture.editor.Cut();
    Require(fixture.Frame(),"menu cut did not mark document dirty");
    fixture.editor.Undo();
    Require(fixture.editor.GetText().find("beta alpha")!=std::string::npos,"menu undo did not restore cut");
    Require(fixture.Frame(),"menu undo change was lost before rendering");
    fixture.editor.Redo();
    Require(fixture.Frame(),"menu redo change was lost before rendering");
    fixture.editor.GoToLine(2);
    fixture.editor.Paste();
    Require(fixture.Frame(),"menu paste did not mark document dirty");
    fixture.editor.GoToLine(1000000);
    Require(fixture.editor.CursorLine()==2,"out-of-range line was not clamped");
    fixture.editor.SetText("small");
    Require(fixture.editor.CursorLine()==1 && fixture.editor.CursorColumn()==1,"document switch retained old caret");
    fixture.editor.Undo();
    fixture.Frame();
    Require(fixture.editor.GetText()=="small","document switch retained undo history");
    fixture.editor.Cut();
    clipboard.clear();
    fixture.editor.Paste();
    Require(!fixture.Frame(),"empty cut/paste marked document dirty");
    Require(!fixture.editor.FindNext("missing"),"find invented a missing match");
}
}

int main()
{
    try {
        EditorFixture fixture;
        CheckNativeInputAndFocus(fixture);
        CheckDocumentActions(fixture);
        std::cout<<"CodeEditor: SDL text activation, focus, UTF-8, IME caret, undo, read-only and document actions passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"CodeEditor regression: "<<error.what()<<'\n';
        return 1;
    }
}
