// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/JsonValue.h"

namespace Concord::AssetJson {

const Value* Value::Find(std::string_view key) const noexcept
{
    if (type != Type::Object) return nullptr;
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

const Value* Value::At(usize index) const noexcept
{
    return type == Type::Array && index < array.size() ? &array[index] : nullptr;
}

std::string_view Value::String(std::string_view fallback) const noexcept
{
    return type == Type::String ? std::string_view(string) : fallback;
}

f64 Value::Number(f64 fallback) const noexcept
{
    return type == Type::Number ? number : fallback;
}

bool Value::Bool(bool fallback) const noexcept
{
    return type == Type::Bool ? boolean : fallback;
}

} // namespace Concord::AssetJson
