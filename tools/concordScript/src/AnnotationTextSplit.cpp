// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationText.h"

#include "AnnotationTextLiterals.h"
#include "AnnotationTextTemplates.h"

#include "AnnotationValues.h"

#include <cctype>

namespace ConcordScript::AnnotationText {

using AnnotationValues::Trim;
using AnnotationTextLiterals::RawStringEnd;
using AnnotationTextTemplates::IsTemplateOpen;

std::vector<std::string> SplitTopLevel(const std::string& text)
{
    std::vector<std::string> values;
    size_t begin = 0;
    int round = 0;
    int square = 0;
    int curly = 0;
    int angle = 0;
    char quote = 0;
    bool escaped = false;
    bool lineComment = false;
    bool blockComment = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const char character = text[i];
        if (lineComment) {
            if (character == '\n') lineComment = false;
            continue;
        }
        if (blockComment) {
            if (character == '*' && i + 1 < text.size() && text[i + 1] == '/') {
                blockComment = false;
                ++i;
            }
            continue;
        }
        if (quote != 0) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == quote) quote = 0;
            continue;
        }
        if (const size_t rawEnd = RawStringEnd(text, i); rawEnd != 0) {
            i = rawEnd - 1;
            continue;
        }
        if (character == '\'' || character == '"') { quote = character; continue; }
        if (character == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            lineComment = true;
            ++i;
            continue;
        }
        if (character == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            blockComment = true;
            ++i;
            continue;
        }
        if (character == '(') ++round;
        else if (character == ')') --round;
        else if (character == '[') ++square;
        else if (character == ']') --square;
        else if (character == '{') ++curly;
        else if (character == '}') --curly;
        else if (character == '<' && IsTemplateOpen(text, i)) ++angle;
        else if (character == '>' && angle > 0) --angle;
        else if (character == ',' && round == 0 && square == 0 && curly == 0 && angle == 0) {
            values.push_back(Trim(text.substr(begin, i - begin)));
            begin = i + 1;
        }
    }
    values.push_back(Trim(text.substr(begin)));
    return values;
}

} // namespace ConcordScript::AnnotationText
