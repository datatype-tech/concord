// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationTextTemplates.h"

#include <cctype>

namespace ConcordScript::AnnotationTextTemplates {

namespace {

char PreviousNonSpace(const std::string& text, std::size_t position)
{
    while (position > 0) {
        const char character = text[--position];
        if (!std::isspace(static_cast<unsigned char>(character))) return character;
    }
    return '\0';
}

char NextNonSpace(const std::string& text, std::size_t position)
{
    for (++position; position < text.size(); ++position) {
        if (!std::isspace(static_cast<unsigned char>(text[position]))) return text[position];
    }
    return '\0';
}

} // namespace

bool IsTemplateOpen(const std::string& text, std::size_t position)
{
    if (position + 1 >= text.size() || text[position + 1] == '<' ||
        text[position + 1] == '=') return false;
    const char previous = PreviousNonSpace(text, position);
    if (!(std::isalnum(static_cast<unsigned char>(previous)) || previous == '_' ||
          previous == ':' || previous == '>' || previous == ')')) return false;
    const char next = NextNonSpace(text, position);
    if (next == '\0' || next == ',' || next == ')' || next == ']' || next == '}') return false;

    int nested = 1;
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = position + 1; index < text.size(); ++index) {
        const char character = text[index];
        if (quote != 0) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == quote) quote = 0;
            continue;
        }
        if (character == '\'' || character == '"') { quote = character; continue; }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            const std::size_t newline = text.find('\n', index + 2);
            if (newline == std::string::npos) return false;
            index = newline;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '*') {
            const std::size_t close = text.find("*/", index + 2);
            if (close == std::string::npos) return false;
            index = close + 1;
            continue;
        }
        if (character == '<' && index + 1 < text.size() && text[index + 1] != '<' &&
            text[index + 1] != '=') {
            ++nested;
        } else if (character == '>') {
            if (--nested == 0) {
                const char after = NextNonSpace(text, index);
                return after == '\0' || after == ',' || after == ')' || after == ']' ||
                       after == '}' || after == ':' || after == '>' || after == '*' ||
                       after == '&' || after == '=' || after == ';' || after == '+' ||
                       after == '-' || after == '/' || after == '%';
            }
        } else if ((character == ')' || character == ']' || character == '}') && nested == 1) {
            return false;
        }
    }
    return false;
}

} // namespace ConcordScript::AnnotationTextTemplates
