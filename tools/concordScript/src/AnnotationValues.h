#ifndef CONCORDSCRIPT_ANNOTATIONVALUES_H
#define CONCORDSCRIPT_ANNOTATIONVALUES_H

#include "Ast.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript::AnnotationValues {

/** Removes leading and trailing whitespace from an annotation value. */
std::string Trim(const std::string& text);

/** Returns a case-folded value using the ASCII rules used by the grammar. */
std::string Lower(std::string text);

/** Removes one matching pair of single or double quotes. */
std::string Unquote(std::string value);

/** Decodes the small escape subset useful in annotation strings. */
std::string DecodeQuoted(std::string value);

/** Finds a case-insensitive named argument, or returns nullptr. */
const AnnotationArg* FindArg(const Annotation& annotation, const std::string& key);

/** Returns a named argument without surrounding quotes, or an empty string. */
std::string NamedValue(const Annotation& annotation, const std::string& key);

/** Returns the zero-based positional argument without surrounding quotes. */
std::string PositionalArg(const Annotation& annotation, size_t index);

/** Looks up a named value and falls back to a positional value. */
std::string ArgValue(const Annotation& annotation, const std::string& key,
                     size_t positional = 0);

/** Collects all positional values in source order. */
std::vector<std::string> PositionalValues(const Annotation& annotation);

} // namespace ConcordScript::AnnotationValues

#endif // CONCORDSCRIPT_ANNOTATIONVALUES_H
