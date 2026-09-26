// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CCodeEditor.h"
#include <TextEditor.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace Concord {
struct CodeEditor::Impl {
    TextEditor editor;
    TextEditor::ErrorMarkers errors;
    bool focused=false,changed=false;
    std::string search;
    std::string originalText;
    bool useCrLf=false;
    size_t searchOffset=0;

    std::string NormalizedText() const
    {
        std::string text=editor.GetText();
        /** The vendor accessor appends a synthetic newline after its final logical line. */
        if(!text.empty() && text.back()=='\n')text.pop_back();
        return text;
    }

    /** Matches the vendor editor's tab stops and gutter without copying the whole document. */
    ImVec2 CaretPosition(const ImVec2& origin) const
    {
        const auto caret=editor.GetCursorPosition();
        const std::string line=editor.GetCurrentLineText();
        float distance=0.0f;
        const float tabWidth=ImGui::CalcTextSize(" ").x*editor.GetTabSize();
        int column=0;
        for(size_t offset=0;offset<line.size() && column<caret.mColumn;) {
            if(line[offset]=='\t') {
                distance=(1.0f+std::floor((1.0f+distance)/tabWidth))*tabWidth;
                column=(column/editor.GetTabSize()+1)*editor.GetTabSize();
                ++offset;
            } else {
                size_t end=offset+1;
                while(end<line.size() && (static_cast<unsigned char>(line[end])&0xc0)==0x80)++end;
                distance+=ImGui::CalcTextSize(line.data()+offset,line.data()+end).x;
                offset=end;
                ++column;
            }
        }
        char gutter[32];
        std::snprintf(gutter,sizeof(gutter)," %d ",editor.GetTotalLines());
        return {origin.x+ImGui::CalcTextSize(gutter).x+10.0f+distance,
                origin.y+caret.mLine*ImGui::GetTextLineHeightWithSpacing()};
    }
};
CodeEditor::CodeEditor() : m_impl(std::make_unique<Impl>())
{
    auto language=TextEditor::LanguageDefinition::CPlusPlus();
    language.mName="ConcordScript";
    for (const char* keyword : {"use","var","extend","entry","register","component","system","update","startup","include","cvm"})
        language.mKeywords.insert(keyword);
    m_impl->editor.SetLanguageDefinition(language);
    m_impl->editor.SetShowWhitespaces(false);
    m_impl->editor.SetTabSize(4);
    m_impl->editor.SetImGuiChildIgnored(true);
    auto palette=TextEditor::GetDarkPalette();
    using Index=TextEditor::PaletteIndex;
    palette[static_cast<size_t>(Index::Background)]=IM_COL32(19,22,29,255);
    palette[static_cast<size_t>(Index::Default)]=IM_COL32(215,223,237,255);
    palette[static_cast<size_t>(Index::Keyword)]=IM_COL32(182,157,255,255);
    palette[static_cast<size_t>(Index::String)]=IM_COL32(166,208,148,255);
    palette[static_cast<size_t>(Index::Number)]=IM_COL32(237,188,129,255);
    palette[static_cast<size_t>(Index::Comment)]=palette[static_cast<size_t>(Index::MultiLineComment)]=IM_COL32(115,135,147,255);
    palette[static_cast<size_t>(Index::LineNumber)]=IM_COL32(89,102,125,255);
    palette[static_cast<size_t>(Index::CurrentLineFill)]=IM_COL32(32,38,51,255);
    palette[static_cast<size_t>(Index::CurrentLineFillInactive)]=IM_COL32(25,29,39,255);
    palette[static_cast<size_t>(Index::CurrentLineEdge)]=IM_COL32(0,0,0,0);
    palette[static_cast<size_t>(Index::Selection)]=IM_COL32(66,84,135,180);
    m_impl->editor.SetPalette(palette);
}
CodeEditor::~CodeEditor()=default;
void CodeEditor::SetLanguage(bool cpp)
{
    if (cpp) {
        m_impl->editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
        return;
    }
    auto language=TextEditor::LanguageDefinition::CPlusPlus();
    language.mName="ConcordScript";
    for(const char* keyword:{"use","var","extend","entry","register","component","system","update","startup","include","cvm"})
        language.mKeywords.insert(keyword);
    m_impl->editor.SetLanguageDefinition(language);
}
void CodeEditor::SetText(const std::string& text)
{
    m_impl->editor.SetText(text);
    m_impl->editor.SetCursorPosition({0,0});
    m_impl->editor.SetSelection({0,0},{0,0});
    m_impl->originalText=text;
    size_t crLf=0,lineFeeds=0;
    for(size_t index=0;index<text.size();++index) {
        if(text[index]=='\n') {
            ++lineFeeds;
            if(index>0 && text[index-1]=='\r')++crLf;
        }
    }
    m_impl->useCrLf=crLf>lineFeeds-crLf;
    m_impl->search.clear();
    m_impl->searchOffset=0;
    m_impl->changed=false;
}
void CodeEditor::SetReadOnly(bool readOnly) { m_impl->editor.SetReadOnly(readOnly); }
std::string CodeEditor::GetText() const
{
    const std::string text=m_impl->NormalizedText();
    size_t position=0;
    bool unchanged=true;
    for(char original:m_impl->originalText) {
        if(original=='\r')continue;
        if(position==text.size() || text[position++]!=original) { unchanged=false;break; }
    }
    if(unchanged && position==text.size())return m_impl->originalText;
    if(!m_impl->useCrLf)return text;
    std::string result;
    result.reserve(text.size()+std::count(text.begin(),text.end(),'\n'));
    for(char character:text) {
        if(character=='\n')result+='\r';
        result+=character;
    }
    return result;
}
bool CodeEditor::Render(const char* id)
{
    const bool visible=ImGui::BeginChild(id,{},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar|ImGuiWindowFlags_NoMove);
    m_impl->focused=false;
    bool changed=m_impl->changed;
    if(visible) {
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        m_impl->editor.SetHandleKeyboardInputs(!ImGui::GetIO().AppFocusLost);
        m_impl->editor.Render(id);
        changed|=m_impl->editor.IsTextChanged();
        m_impl->focused=ImGui::IsWindowFocused() && !ImGui::GetIO().AppFocusLost;
        const auto& io=ImGui::GetIO();
        if(m_impl->focused && io.KeyCtrl && io.KeyShift && !io.KeyAlt &&
           ImGui::IsKeyPressed(ImGuiKey_Z) && m_impl->editor.CanRedo()) {
            m_impl->editor.Redo();
            changed=true;
        }
        if(m_impl->focused && !m_impl->editor.IsReadOnly()) {
            /** SDL3 starts text input through the platform IME callback, not WantTextInput alone. */
            auto& context=*ImGui::GetCurrentContext();
            const auto* window=ImGui::GetCurrentWindow();
            ImVec2 caret=m_impl->CaretPosition(origin);
            caret.x=std::clamp(caret.x,window->InnerClipRect.Min.x,window->InnerClipRect.Max.x);
            caret.y=std::clamp(caret.y,window->InnerClipRect.Min.y,
                std::max(window->InnerClipRect.Min.y,window->InnerClipRect.Max.y-ImGui::GetFontSize()));
            context.WantTextInputNextFrame=1;
            context.PlatformImeData.WantVisible=true;
            context.PlatformImeData.InputPos=caret;
            context.PlatformImeData.InputLineHeight=ImGui::GetFontSize();
            context.PlatformImeViewport=ImGui::GetWindowViewport()->ID;
            ImGui::SetNextFrameWantCaptureKeyboard(true);
        }
    }
    ImGui::EndChild();
    m_impl->changed=false;
    return changed;
}
void CodeEditor::ClearErrors() { m_impl->errors.clear(); m_impl->editor.SetErrorMarkers(m_impl->errors); }
void CodeEditor::SetError(int line,const std::string& message)
{
    m_impl->errors[line]=message; m_impl->editor.SetErrorMarkers(m_impl->errors);
}
void CodeEditor::GoToLine(int line)
{
    const TextEditor::Coordinates caret{std::clamp(line,1,m_impl->editor.GetTotalLines())-1,0};
    m_impl->editor.SetCursorPosition(caret);
    m_impl->editor.SetSelection(caret,caret);
}
int CodeEditor::CursorLine() const { return m_impl->editor.GetCursorPosition().mLine+1; }
int CodeEditor::CursorColumn() const { return m_impl->editor.GetCursorPosition().mColumn+1; }
void CodeEditor::Complete(const std::string& label)
{
    if(m_impl->editor.IsReadOnly() || label.empty())return;
    const auto caret=m_impl->editor.GetCursorPosition();
    const std::string line=m_impl->editor.GetCurrentLineText();
    int start=std::min(caret.mColumn,static_cast<int>(line.size()));
    while(start>0 && (std::isalnum(static_cast<unsigned char>(line[start-1])) || line[start-1]=='_'))--start;
    m_impl->editor.SetSelection({caret.mLine,start},caret);
    m_impl->editor.InsertText(label);
    m_impl->changed=true;
}
void CodeEditor::Undo() { if(m_impl->editor.CanUndo()){m_impl->editor.Undo();m_impl->changed=true;} }
void CodeEditor::Redo() { if(m_impl->editor.CanRedo()){m_impl->editor.Redo();m_impl->changed=true;} }
void CodeEditor::Copy() {m_impl->editor.Copy();}
void CodeEditor::Cut()
{
    if(!m_impl->editor.IsReadOnly() && m_impl->editor.HasSelection()) {
        m_impl->editor.Cut();
        m_impl->changed=true;
    }
}
void CodeEditor::Paste()
{
    if(!m_impl->editor.IsReadOnly()) {
        const std::string before=m_impl->editor.GetText();
        m_impl->editor.Paste();
        m_impl->changed|=before!=m_impl->editor.GetText();
    }
}
bool CodeEditor::IsFocused() const {return m_impl->focused;}
bool CodeEditor::FindNext(const std::string& query)
{
    if(query.empty())return false;
    auto text=m_impl->NormalizedText();if(m_impl->search!=query){m_impl->search=query;m_impl->searchOffset=0;}
    auto found=text.find(query,m_impl->searchOffset);if(found==std::string::npos)found=text.find(query);
    if(found==std::string::npos)return false;
    auto coordinates=[&](size_t end) {
        int line=0,column=0;
        for(size_t i=0;i<end;++i) {const unsigned char c=text[i];if(c=='\n'){++line;column=0;}else if(c=='\t')column=(column/4+1)*4;else if((c&0xc0)!=0x80)++column;}
        return TextEditor::Coordinates{line,column};
    };
    const auto start=coordinates(found),end=coordinates(found+query.size());
    m_impl->editor.SetCursorPosition(start);m_impl->editor.SetSelection(start,end);
    m_impl->searchOffset=found+query.size();return true;
}
}
