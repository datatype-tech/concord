#include "AnnotationPassSupport.h"

#include "AnnotationValues.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <utility>

namespace ConcordScript::AnnotationPassSupport {

std::string CppString(std::string value)
{
    value = AnnotationValues::Unquote(std::move(value));
    std::string result = "\"";
    for (const unsigned char character : value) {
        if (character == '\\') result += "\\\\";
        else if (character == '"') result += "\\\"";
        else if (character == '\n') result += "\\n";
        else if (character == '\r') result += "\\r";
        else if (character == '\t') result += "\\t";
        else if (character < 0x20) result += ' ';
        else result += static_cast<char>(character);
    }
    result += '"';
    return result;
}

bool IsIdentifierExpression(const std::string& rawValue)
{
    std::string value = AnnotationValues::Trim(rawValue);
    if (value.empty()) return false;
    size_t index = value.front() == '&' ? 1 : 0;
    if (value.compare(index, 2, "::") == 0) index += 2;
    if (index >= value.size()) return false;
    bool needIdentifier = true;
    for (; index < value.size(); ++index) {
        const char character = value[index];
        if (needIdentifier) {
            if (!(std::isalpha(static_cast<unsigned char>(character)) || character == '_')) {
                return false;
            }
            needIdentifier = false;
        } else if (character == ':' && index + 1 < value.size() && value[index + 1] == ':') {
            ++index;
            needIdentifier = true;
        } else if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_')) {
            return false;
        }
    }
    return !needIdentifier;
}

bool IsCallbackExpression(const std::string& rawValue)
{
    const std::string value = AnnotationValues::Trim(rawValue);
    if (value.empty()) return false;
    for (const unsigned char character : value) {
        if (character < 0x20 && character != '\t') return false;
    }
    return true;
}

bool IsUserDataExpression(const std::string& value)
{
    const std::string trimmed = AnnotationValues::Trim(value);
    return trimmed == "nullptr" || trimmed == "0" || IsIdentifierExpression(trimmed);
}

bool IsOrderLiteral(const std::string& rawValue)
{
    const std::string value = AnnotationValues::Trim(AnnotationValues::Unquote(rawValue));
    if (value.empty()) return false;
    const bool suffix = value.back() == 'u' || value.back() == 'U';
    const size_t end = suffix ? value.size() - 1 : value.size();
    if (end == 0) return false;
    const bool hexadecimal = end > 2 && value[0] == '0' &&
                             (value[1] == 'x' || value[1] == 'X');
    const size_t begin = hexadecimal ? 2 : 0;
    if (begin == end) return false;
    std::uint64_t parsed = 0;
    for (size_t index = begin; index < end; ++index) {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        const int digit = hexadecimal
            ? (std::isdigit(character) ? character - '0'
               : std::tolower(character) - 'a' + 10)
            : (std::isdigit(character) ? character - '0' : -1);
        if (digit < 0 || digit >= (hexadecimal ? 16 : 10)) return false;
        parsed = parsed * static_cast<std::uint64_t>(hexadecimal ? 16 : 10) +
                 static_cast<std::uint64_t>(digit);
        if (parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    }
    return true;
}

std::string NormalizePhase(std::string value)
{
    value = AnnotationValues::Lower(
        AnnotationValues::Trim(AnnotationValues::Unquote(std::move(value))));
    if (value == "initialize" || value == "init" || value == "device_ready" ||
        value == "device-ready") return "Initialize";
    if (value == "before" || value == "before_scene" || value == "before-scene" ||
        value == "beforescene" || value == "pre") return "BeforeScene";
    if (value == "after" || value == "after_scene" || value == "after-scene" ||
        value == "afterscene" || value == "post") return "AfterScene";
    if (value == "shutdown" || value == "destroy" || value == "device_lost" ||
        value == "device-lost") return "Shutdown";
    return {};
}

} // namespace ConcordScript::AnnotationPassSupport
