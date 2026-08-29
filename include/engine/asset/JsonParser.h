// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_JSONPARSER_H
#define CONCORD_ASSET_JSONPARSER_H

#include "engine/asset/JsonValue.h"

#include <string_view>

namespace Concord::AssetJson {

/** Small recursive-descent reader kept private to the model importer. */
class Parser {
public:
    Parser(std::string_view source, std::string& error,
           usize& errorLine, usize& errorColumn) noexcept;
    bool Read(Value& value) noexcept;

private:
    bool ReadValue(Value& value) noexcept;
    bool ReadObject(Value& value) noexcept;
    bool ReadArray(Value& value) noexcept;
    bool ReadString(std::string& value) noexcept;
    bool ReadNumber(Value& value) noexcept;
    bool ReadLiteral(std::string_view literal, Value& value, Type type) noexcept;
    void SkipSpace() noexcept;
    bool Fail(std::string_view message) noexcept;
    char Peek() const noexcept;
    char Take() noexcept;

    std::string_view m_source;
    usize m_offset = 0;
    usize m_line = 1;
    usize m_column = 1;
    std::string& m_error;
    usize& m_errorLine;
    usize& m_errorColumn;
};

} // namespace Concord::AssetJson

#endif // CONCORD_ASSET_JSONPARSER_H
