#include "KeywordSubstitution.h"

#include <cctype>
#include <iterator>
#include <string_view>
#include <utility>

namespace ConcordScript {

namespace {

/** Whether `c` can appear inside a C++ identifier. */
bool IsIdentChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

/** Returns the end of a quoted C++ literal, including its closing quote. */
size_t QuotedEnd(const std::string& text, size_t begin, char quote)
{
    for (size_t index = begin + 1; index < text.size(); ++index) {
        if (text[index] == '\\') {
            if (index + 1 < text.size()) ++index;
        } else if (text[index] == quote) {
            return index + 1;
        }
    }
    return text.size();
}

/** Returns the end of a line or block comment, preserving all comment text. */
size_t CommentEnd(const std::string& text, size_t begin, bool line)
{
    if (line) {
        const size_t newline = text.find('\n', begin + 2);
        return newline == std::string::npos ? text.size() : newline;
    }
    const size_t close = text.find("*/", begin + 2);
    return close == std::string::npos ? text.size() : close + 2;
}

/**
 * Recognizes C++ raw string prefixes and returns the complete literal span.
 * An unterminated raw literal consumes the remainder so its payload is never
 * rewritten into a different token stream.
 */
size_t RawStringEnd(const std::string& text, size_t begin)
{
    if (begin != 0 && IsIdentChar(text[begin - 1])) return 0;
    size_t quote = begin;
    if (text.compare(begin, 4, "u8R\"") == 0) quote += 3;
    else if (text.compare(begin, 3, "uR\"") == 0 ||
             text.compare(begin, 3, "UR\"") == 0 ||
             text.compare(begin, 3, "LR\"") == 0) quote += 2;
    else if (text.compare(begin, 2, "R\"") == 0) quote += 1;
    else return 0;

    const size_t open = text.find('(', quote + 1);
    if (open == std::string::npos || open - quote - 1 > 16) return text.size();
    for (size_t index = quote + 1; index < open; ++index) {
        const char character = text[index];
        if (std::isspace(static_cast<unsigned char>(character)) ||
            character == '\\' || character == '(' || character == ')') {
            return text.size();
        }
    }
    const std::string delimiter = text.substr(quote + 1, open - quote - 1);
    const std::string terminator = ")" + delimiter + "\"";
    const size_t close = text.find(terminator, open + 1);
    return close == std::string::npos ? text.size() : close + terminator.size();
}

/** Rewrites a keyword only in code, never in literals or comments. */
std::string ReplaceCodeKeywords(const std::string& text, bool includeAccessMarkers)
{
    constexpr std::pair<std::string_view, std::string_view> allReplacements[] = {
        {"var", "auto"}, {"pub", "public:"}, {"priv", "private:"},
        {"prot", "protected:"}};
    constexpr std::pair<std::string_view, std::string_view> varOnly[] = {{"var", "auto"}};
    const auto* replacements = includeAccessMarkers ? allReplacements : varOnly;
    const size_t replacementCount = includeAccessMarkers ? std::size(allReplacements) : 1;
    std::string out;
    out.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        const char character = text[index];
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '/') {
            const size_t end = CommentEnd(text, index, true);
            out.append(text, index, end - index);
            index = end;
            continue;
        }
        if (character == '/' && index + 1 < text.size() && text[index + 1] == '*') {
            const size_t end = CommentEnd(text, index, false);
            out.append(text, index, end - index);
            index = end;
            continue;
        }
        if (character == '"' || character == '\'') {
            const size_t end = QuotedEnd(text, index, character);
            out.append(text, index, end - index);
            index = end;
            continue;
        }
        if (const size_t end = RawStringEnd(text, index); end != 0) {
            out.append(text, index, end - index);
            index = end;
            continue;
        }
        bool replaced = false;
        for (size_t replacementIndex = 0; replacementIndex < replacementCount; ++replacementIndex) {
            const auto [word, replacement] = replacements[replacementIndex];
            const bool matches = text.compare(index, word.size(), word) == 0;
            const bool before = index == 0 || !IsIdentChar(text[index - 1]);
            const bool after = index + word.size() >= text.size() ||
                               !IsIdentChar(text[index + word.size()]);
            if (matches && before && after) {
                out += replacement;
                index += word.size();
                replaced = true;
                break;
            }
        }
        if (!replaced) out += text[index++];
    }
    return out;
}

} // namespace

std::string SubstituteKeywords(const std::string& bodyText, bool isClassBody)
{
    std::string text = ReplaceCodeKeywords(bodyText, isClassBody);

    if (isClassBody) {
        size_t firstNonSpace = text.find_first_not_of(" \t\r\n");
        const bool startsWithLabel = firstNonSpace != std::string::npos &&
            (text.compare(firstNonSpace, 7, "public:") == 0 ||
             text.compare(firstNonSpace, 8, "private:") == 0 ||
             text.compare(firstNonSpace, 10, "protected:") == 0);

        if (!startsWithLabel) {
            text = "public:\n" + text;
        }
    }

    return text;
}

} // namespace ConcordScript
