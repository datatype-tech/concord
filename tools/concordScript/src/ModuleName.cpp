// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "ModuleName.h"

#include <cctype>

namespace ConcordScript::ModuleName {

bool IsValid(const std::string& name)
{
    if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name.front())) ||
                          name.front() == '_')) return false;
    for (const char character : name) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_')) return false;
    }
    static constexpr const char* keywords[] = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool",
        "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl",
        "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
        "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
        "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for",
        "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
        "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static",
        "static_assert", "static_cast", "struct", "switch", "template", "this", "thread_local",
        "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"};
    for (const char* keyword : keywords) {
        if (name == keyword) return false;
    }
    return true;
}

} // namespace ConcordScript::ModuleName
