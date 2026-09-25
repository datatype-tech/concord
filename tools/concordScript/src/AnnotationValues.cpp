#include "AnnotationValues.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace ConcordScript::AnnotationValues {

std::string Trim(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

std::string Unquote(std::string value)
{
    value = Trim(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string DecodeQuoted(std::string value)
{
    value = Unquote(std::move(value));
    std::string decoded;
    decoded.reserve(value.size());
    bool escaped = false;
    for (const char character : value) {
        if (escaped) {
            switch (character) {
            case 'n': decoded += '\n'; break;
            case 'r': decoded += '\r'; break;
            case 't': decoded += '\t'; break;
            default: decoded += '\\'; decoded += character; break;
            }
            escaped = false;
        } else if (character == '\\') {
            escaped = true;
        } else {
            decoded += character;
        }
    }
    if (escaped) decoded += '\\';
    return decoded;
}

const AnnotationArg* FindArg(const Annotation& annotation, const std::string& key)
{
    const std::string wanted = Lower(Trim(key));
    for (const AnnotationArg& argument : annotation.args) {
        if (argument.hasKey && Lower(Trim(argument.key)) == wanted) {
            return &argument;
        }
    }
    return nullptr;
}

std::string NamedValue(const Annotation& annotation, const std::string& key)
{
    if (const AnnotationArg* argument = FindArg(annotation, key)) {
        return Unquote(argument->value);
    }
    return {};
}

std::string PositionalArg(const Annotation& annotation, size_t index)
{
    size_t current = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey && current++ == index) {
            return Unquote(argument.value);
        }
    }
    return {};
}

std::string ArgValue(const Annotation& annotation, const std::string& key, size_t positional)
{
    if (const AnnotationArg* argument = FindArg(annotation, key)) {
        return Unquote(argument->value);
    }
    return PositionalArg(annotation, positional);
}

std::vector<std::string> PositionalValues(const Annotation& annotation)
{
    std::vector<std::string> values;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey) values.push_back(Unquote(argument.value));
    }
    return values;
}

} // namespace ConcordScript::AnnotationValues
