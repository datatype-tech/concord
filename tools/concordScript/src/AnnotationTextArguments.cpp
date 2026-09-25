// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationText.h"

#include "AnnotationTextLiterals.h"
#include "AnnotationTextTemplates.h"

namespace ConcordScript::AnnotationText {

std::size_t FindTopLevelEquals(const std::string& text)
{
    int round = 0;
    int square = 0;
    int curly = 0;
    int angle = 0;
    char quote = 0;
    bool escaped = false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (quote != 0) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == quote) quote = 0;
            continue;
        }
        if (character == '\'' || character == '"') { quote = character; continue; }
        if (const std::size_t rawEnd = AnnotationTextLiterals::RawStringEnd(text, index);
            rawEnd != 0) {
            index = rawEnd - 1;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            const std::size_t newline = text.find('\n', index + 2);
            index = newline == std::string::npos ? text.size() : newline;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '*') {
            const std::size_t close = text.find("*/", index + 2);
            index = close == std::string::npos ? text.size() : close + 1;
            continue;
        }
        if (character == '(') ++round;
        else if (character == ')') --round;
        else if (character == '[') ++square;
        else if (character == ']') --square;
        else if (character == '{') ++curly;
        else if (character == '}') --curly;
        else if (character == '<' && AnnotationTextTemplates::IsTemplateOpen(text, index)) ++angle;
        else if (character == '>' && angle > 0) --angle;
        else if (character == '=' && round == 0 && square == 0 && curly == 0 && angle == 0) {
            const char previous = index == 0 ? '\0' : text[index - 1];
            const char next = index + 1 < text.size() ? text[index + 1] : '\0';
            if (previous != '=' && previous != '!' && previous != '<' && previous != '>' &&
                next != '=' && next != '>') {
                return index;
            }
        }
    }
    return std::string::npos;
}

} // namespace ConcordScript::AnnotationText
