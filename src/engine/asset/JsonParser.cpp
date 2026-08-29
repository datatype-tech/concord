// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/JsonParser.h"

#include <charconv>
#include <cctype>

namespace Concord::AssetJson {

Parser::Parser(std::string_view source, std::string& error,
               usize& errorLine, usize& errorColumn) noexcept
    : m_source(source), m_error(error), m_errorLine(errorLine), m_errorColumn(errorColumn)
{
}

char Parser::Peek() const noexcept
{
    return m_offset < m_source.size() ? m_source[m_offset] : '\0';
}

char Parser::Take() noexcept
{
    const char value = Peek();
    if (value != '\0') {
        ++m_offset;
        if (value == '\n') { ++m_line; m_column = 1; } else { ++m_column; }
    }
    return value;
}

void Parser::SkipSpace() noexcept
{
    while (std::isspace(static_cast<unsigned char>(Peek())) != 0) Take();
}

bool Parser::Fail(std::string_view message) noexcept
{
    if (m_error.empty()) {
        m_error.assign(message);
        m_errorLine = m_line;
        m_errorColumn = m_column;
    }
    return false;
}

bool Parser::Read(Value& value) noexcept
{
    SkipSpace();
    if (!ReadValue(value)) return false;
    SkipSpace();
    return Peek() == '\0' ? true : Fail("trailing characters after JSON value");
}

bool Parser::ReadValue(Value& value) noexcept
{
    SkipSpace();
    switch (Peek()) {
    case '{': return ReadObject(value);
    case '[': return ReadArray(value);
    case '"': value.type = Type::String; return ReadString(value.string);
    case 't': return ReadLiteral("true", value, Type::Bool);
    case 'f': return ReadLiteral("false", value, Type::Bool);
    case 'n': return ReadLiteral("null", value, Type::Null);
    default:
        if (Peek() == '-' || std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
            return ReadNumber(value);
        }
        return Fail("expected JSON value");
    }
}

bool Parser::ReadLiteral(std::string_view literal, Value& value, Type type) noexcept
{
    if (m_source.substr(m_offset, literal.size()) != literal) return Fail("invalid JSON literal");
    for (char ignored : literal) { (void)ignored; Take(); }
    value = Value{};
    value.type = type;
    value.boolean = type == Type::Bool && literal == "true";
    return true;
}

bool Parser::ReadNumber(Value& value) noexcept
{
    const usize start = m_offset;
    if (Peek() == '-') Take();
    if (Peek() == '0') {
        Take();
        if (std::isdigit(static_cast<unsigned char>(Peek())) != 0) return Fail("leading zero in JSON number");
    } else if (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
        while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) Take();
    } else {
        return Fail("invalid JSON number");
    }
    if (Peek() == '.') {
        Take();
        if (std::isdigit(static_cast<unsigned char>(Peek())) == 0) return Fail("invalid JSON fraction");
        while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) Take();
    }
    if (Peek() == 'e' || Peek() == 'E') {
        Take();
        if (Peek() == '+' || Peek() == '-') Take();
        if (std::isdigit(static_cast<unsigned char>(Peek())) == 0) return Fail("invalid JSON exponent");
        while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) Take();
    }
    const std::string_view token = m_source.substr(start, m_offset - start);
    f64 number = 0.0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return Fail("number out of range");
    value = Value{};
    value.type = Type::Number;
    value.number = number;
    return true;
}

bool Parse(std::string_view source, Value& result, std::string& error,
           usize& errorLine, usize& errorColumn) noexcept
{
    error.clear(); errorLine = 0; errorColumn = 0; result = Value{};
    Parser parser(source, error, errorLine, errorColumn);
    return parser.Read(result);
}

} // namespace Concord::AssetJson
