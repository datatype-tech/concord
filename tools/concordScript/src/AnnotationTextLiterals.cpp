// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationTextLiterals.h"

#include <cctype>

namespace ConcordScript::AnnotationTextLiterals {

namespace {

bool IsIdentifierCharacter(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
}

} // namespace

size_t RawStringEnd(const std::string& text, size_t position)
{
    if (position >= text.size() || (position != 0 && IsIdentifierCharacter(text[position - 1]))) {
        return 0;
    }
    size_t quote = position;
    if (text.compare(position, 4, "u8R\"") == 0) quote += 3;
    else if (text.compare(position, 3, "uR\"") == 0 ||
             text.compare(position, 3, "UR\"") == 0 ||
             text.compare(position, 3, "LR\"") == 0) quote += 2;
    else if (text.compare(position, 2, "R\"") == 0) quote += 1;
    else return 0;

    const size_t open = text.find('(', quote + 1);
    if (open == std::string::npos) return text.size();
    const size_t delimiterLength = open - quote - 1;
    if (delimiterLength > 16) return text.size();
    for (size_t index = quote + 1; index < open; ++index) {
        const char character = text[index];
        if (std::isspace(static_cast<unsigned char>(character)) || character == '\\' ||
            character == '(' || character == ')' || character == '"') {
            return text.size();
        }
    }
    const std::string delimiter = text.substr(quote + 1, delimiterLength);
    const std::string terminator = ")" + delimiter + '"';
    const size_t close = text.find(terminator, open + 1);
    return close == std::string::npos ? text.size() : close + terminator.size();
}

} // namespace ConcordScript::AnnotationTextLiterals
