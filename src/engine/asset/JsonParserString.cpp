// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/JsonParser.h"

namespace Concord::AssetJson {
namespace {

void AppendUtf8(std::string& out, u32 codepoint)
{
    if (codepoint <= 0x7Fu) out.push_back(static_cast<char>(codepoint));
    else if (codepoint <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
}

} // namespace

bool Parser::ReadString(std::string& value) noexcept
{
    if (Take() != '"') return Fail("expected JSON string");
    value.clear();
    while (true) {
        const char character = Take();
        if (character == '\0') return Fail("unterminated JSON string");
        if (character == '"') return true;
        if (static_cast<unsigned char>(character) < 0x20u) return Fail("control character in JSON string");
        if (character != '\\') { value.push_back(character); continue; }
        const char escaped = Take();
        switch (escaped) {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u': {
            u32 codepoint = 0;
            for (u32 i = 0; i < 4; ++i) {
                const char hex = Take();
                const u32 digit = hex >= '0' && hex <= '9' ? static_cast<u32>(hex - '0')
                    : hex >= 'a' && hex <= 'f' ? static_cast<u32>(hex - 'a' + 10)
                    : hex >= 'A' && hex <= 'F' ? static_cast<u32>(hex - 'A' + 10) : 16u;
                if (digit > 15u) return Fail("invalid unicode escape");
                codepoint = (codepoint << 4) | digit;
            }
            AppendUtf8(value, codepoint);
            break;
        }
        default: return Fail("invalid JSON escape");
        }
    }
}

} // namespace Concord::AssetJson
