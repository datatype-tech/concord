// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/JsonParser.h"

namespace Concord::AssetJson {

bool Parser::ReadObject(Value& value) noexcept
{
    Take(); value = Value{}; value.type = Type::Object; SkipSpace();
    if (Peek() == '}') { Take(); return true; }
    while (true) {
        if (Peek() != '"') return Fail("object key must be a string");
        std::string key;
        if (!ReadString(key)) return false;
        SkipSpace();
        if (Take() != ':') return Fail("expected ':' after object key");
        Value child;
        if (!ReadValue(child)) return false;
        if (!value.object.emplace(std::move(key), std::move(child)).second) {
            return Fail("duplicate JSON object key");
        }
        SkipSpace();
        const char separator = Take();
        if (separator == '}') return true;
        if (separator != ',') return Fail("expected ',' or '}' in object");
        SkipSpace();
    }
}

bool Parser::ReadArray(Value& value) noexcept
{
    Take(); value = Value{}; value.type = Type::Array; SkipSpace();
    if (Peek() == ']') { Take(); return true; }
    while (true) {
        Value child;
        if (!ReadValue(child)) return false;
        value.array.push_back(std::move(child));
        SkipSpace();
        const char separator = Take();
        if (separator == ']') return true;
        if (separator != ',') return Fail("expected ',' or ']' in array");
        SkipSpace();
    }
}

} // namespace Concord::AssetJson
