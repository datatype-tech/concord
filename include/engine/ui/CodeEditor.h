// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_CODEEDITOR_H
#define CONCORD_CODEEDITOR_H
#include "Concord/CExport.h"
#include <memory>
#include <string>
namespace Concord {
/** Syntax-colored ConcordScript document with selection, undo and line diagnostics. */
class CENGINE_API CodeEditor {
public:
    CodeEditor();
    ~CodeEditor();
    CodeEditor(const CodeEditor&)=delete;
    CodeEditor& operator=(const CodeEditor&)=delete;
    /** Replaces the document and resets its caret, selection, search and undo history. */
    void SetText(const std::string& text);
    /** Protects generated documents while retaining selection and copying. */
    void SetReadOnly(bool readOnly);
    /** Switches between ConcordScript and the vendor's C++ lexer. */
    void SetLanguage(bool cpp);
    /** Preserves unchanged source bytes and uses its predominant newline convention after edits. */
    [[nodiscard]] std::string GetText() const;
    /** Draws into the current UI window; true if text changed during this frame. */
    bool Render(const char* id);
    /** Clears all prior line diagnostics. */
    void ClearErrors();
    /** Marks a one-based line with a compiler diagnostic. */
    void SetError(int line,const std::string& message);
    /** Moves the caret to a one-based source line, clamped to the document. */
    void GoToLine(int line);
    [[nodiscard]] int CursorLine() const;
    [[nodiscard]] int CursorColumn() const;
    /** Replaces the identifier immediately before the caret with a completion. */
    void Complete(const std::string& label);
    void Undo();
    void Redo();
    /** Clipboard actions preserve the document's undo history. */
    void Copy();
    void Cut();
    void Paste();
    /** Selects the next literal match, wrapping at the end of the document. */
    bool FindNext(const std::string& text);
    [[nodiscard]] bool IsFocused() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
