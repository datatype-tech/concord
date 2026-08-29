// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_JSONVALUE_H
#define CONCORD_ASSET_JSONVALUE_H

#include "engine/core/Types.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Concord::AssetJson {

/** Minimal JSON DOM used by the dependency-free glTF importer. */
enum class Type { Null, Bool, Number, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool boolean = false;
    f64 number = 0.0;
    std::string string;
    std::vector<Value> array;
    std::map<std::string, Value, std::less<>> object;

    [[nodiscard]] bool Is(Type expected) const noexcept { return type == expected; }
    [[nodiscard]] const Value* Find(std::string_view key) const noexcept;
    [[nodiscard]] const Value* At(usize index) const noexcept;
    [[nodiscard]] std::string_view String(std::string_view fallback = {}) const noexcept;
    [[nodiscard]] f64 Number(f64 fallback = 0.0) const noexcept;
    [[nodiscard]] bool Bool(bool fallback = false) const noexcept;
};

/** Parses one complete JSON document and reports the first malformed token. */
bool Parse(std::string_view source, Value& result, std::string& error,
           usize& errorLine, usize& errorColumn) noexcept;

} // namespace Concord::AssetJson

#endif // CONCORD_ASSET_JSONVALUE_H
