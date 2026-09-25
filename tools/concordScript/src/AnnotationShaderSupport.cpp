// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationShaderSupport.h"

#include "AnnotationShaderStages.h"
#include "AnnotationValues.h"

#include <cctype>

namespace ConcordScript::AnnotationShaderSupport {

namespace {

bool IsPositionalQuoted(const Annotation& annotation, std::size_t wanted)
{
    std::size_t index = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey && index++ == wanted) return argument.quoted;
    }
    return false;
}

} // namespace

bool IsIdentifier(const std::string& value)
{
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) ||
                          value.front() == '_')) return false;
    for (const char character : value) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_')) {
            return false;
        }
    }
    return true;
}

bool IsLanguageToken(const std::string& value)
{
    if (value.empty()) return false;
    for (const char character : value) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_' ||
              character == '-' || character == '.')) return false;
    }
    return true;
}

std::size_t PositionalStageIndex(const Annotation& annotation,
                                 const std::vector<std::string>& values,
                                 bool& ambiguous)
{
    ambiguous = false;
    const bool first = !values.empty() &&
                       AnnotationShaderStages::IsSupported(
                           AnnotationShaderStages::Normalize(values[0]));
    const bool second = values.size() > 1 &&
                        AnnotationShaderStages::IsSupported(
                            AnnotationShaderStages::Normalize(values[1]));
    if (first && second) {
        const bool firstQuoted = IsPositionalQuoted(annotation, 0);
        const bool secondQuoted = IsPositionalQuoted(annotation, 1);
        if (firstQuoted != secondQuoted) return firstQuoted ? 1 : 0;
        ambiguous = true;
        return 0;
    }
    if (first) return 0;
    if (second) return 1;
    return std::string::npos;
}

} // namespace ConcordScript::AnnotationShaderSupport
