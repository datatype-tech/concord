#ifndef CONCORDSCRIPT_ANNOTATIONTEXT_H
#define CONCORDSCRIPT_ANNOTATIONTEXT_H

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript::AnnotationText {

/** Splits comma-separated text while respecting quotes and nested delimiters. */
std::vector<std::string> SplitTopLevel(const std::string& text);

/** Finds a standalone key/value `=` outside literals, comments, and delimiters. */
std::size_t FindTopLevelEquals(const std::string& text);

/** Appends a validated quoted or system include to a generated stream. */
void AppendInclude(std::string& destination, const std::string& rawPath, bool system);

/** Returns whether an include path can be emitted without changing syntax. */
bool IsIncludePathSafe(const std::string& path);

/** Replaces comment terminators before annotation text is placed in a comment. */
std::string SanitizeComment(std::string text);

/** Converts arbitrary user text into a stable path or identifier component. */
std::string SafePart(std::string value, const std::string& fallback);

/** Converts Windows separators to deterministic POSIX separators. */
std::string NormalizePath(std::string path);

/** Rejects absolute paths and traversal components for generated sidecars. */
bool IsSafePath(const std::string& path);

} // namespace ConcordScript::AnnotationText

#endif // CONCORDSCRIPT_ANNOTATIONTEXT_H
