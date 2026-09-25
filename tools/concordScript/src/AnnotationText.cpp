#include "AnnotationText.h"

#include "AnnotationValues.h"

#include <utility>

namespace ConcordScript::AnnotationText {

using AnnotationValues::Trim;
using AnnotationValues::Unquote;

bool IsIncludePathSafe(const std::string& path)
{
    return !path.empty() && path.find('\0') == std::string::npos &&
           path.find_first_of("\r\n<>\";|?*") == std::string::npos;
}

void AppendInclude(std::string& destination, const std::string& rawPath, bool system)
{
    const std::string path = Unquote(rawPath);
    if (!IsIncludePathSafe(path)) return;
    destination += "#include " + std::string(system ? "<" : "\"") + path +
                   (system ? ">\n" : "\"\n");
}

std::string SanitizeComment(std::string text)
{
    size_t position = 0;
    while ((position = text.find("*/", position)) != std::string::npos) {
        text.replace(position, 2, "* /");
        position += 3;
    }
    return text;
}

std::string SafePart(std::string value, const std::string& fallback)
{
    value = Unquote(std::move(value));
    std::string result;
    for (const char character : value) {
        if (std::isalnum(static_cast<unsigned char>(character)) || character == '_' || character == '-') {
            result += character;
        } else {
            result += '_';
        }
    }
    return result.empty() ? fallback : result;
}

std::string NormalizePath(std::string path)
{
    for (char& character : path) {
        if (character == '\\') character = '/';
    }
    return path;
}

bool IsSafePath(const std::string& path)
{
    if (path.empty() || path.front() == '/' || path.front() == '\\' ||
        path.find(':') != std::string::npos ||
        path.find('\0') != std::string::npos ||
        path.find_first_of("\r\n;\"<>|?*") != std::string::npos) {
        return false;
    }
    size_t begin = 0;
    while (begin <= path.size()) {
        const size_t end = path.find_first_of("/\\", begin);
        const std::string part = path.substr(begin, end == std::string::npos ? end : end - begin);
        if (part == ".." || part == "." || part.empty()) return false;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return true;
}

} // namespace ConcordScript::AnnotationText
