// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/ConcordScriptLanguage.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace Concord::Editor {

std::vector<std::string> ConcordScriptCompletions(std::string_view source,
                                                  std::string_view prefix)
{
    if (prefix.empty() || prefix.size() > 128) return {};
    static constexpr std::string_view words[] = {
        "use", "var", "extend", "entry", "register", "component", "system",
        "update", "startup", "include", "cvm", "class", "struct", "public",
        "private", "return", "if", "else", "for", "while", "true", "false",
        "Concord", "Scene", "Game", "Window", "Transform", "Spawn", "Query"
    };
    std::set<std::string> matches;
    for (const auto word : words)
        if (word.starts_with(prefix) && word != prefix) matches.emplace(word);

    // Ignore strings and comments so their prose does not pollute suggestions.
    enum class State { Code, String, LineComment, BlockComment };
    State state = State::Code;
    for (size_t i = 0; i < source.size();) {
        const char c = source[i];
        const char next = i + 1 < source.size() ? source[i + 1] : '\0';
        if (state == State::LineComment) {
            if (c == '\n') state = State::Code;
            ++i; continue;
        }
        if (state == State::BlockComment) {
            if (c == '*' && next == '/') { state = State::Code; i += 2; }
            else ++i;
            continue;
        }
        if (state == State::String) {
            if (c == '\\' && next) i += 2;
            else { if (c == '"') state = State::Code; ++i; }
            continue;
        }
        if (c == '"') { state = State::String; ++i; continue; }
        if (c == '/' && next == '/') { state = State::LineComment; i += 2; continue; }
        if (c == '/' && next == '*') { state = State::BlockComment; i += 2; continue; }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const size_t start = i++;
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            const auto word = source.substr(start, i - start);
            if (word.size() <= 128 && word.starts_with(prefix) && word != prefix)
                matches.emplace(word);
        } else ++i;
    }
    std::vector<std::string> result;
    for (const auto& match : matches) {
        if (result.size() == 32) break;
        result.push_back(match);
    }
    return result;
}

} // namespace Concord::Editor
