// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationValidation.h"

#include "AnnotationText.h"
#include "AnnotationValues.h"

#include <utility>

namespace ConcordScript::AnnotationValidation {
namespace {

void AddError(const Annotation& annotation,
              std::vector<GeneratedDiagnostic>& diagnostics, std::string message)
{
    diagnostics.push_back(GeneratedDiagnostic{
        .message = std::move(message), .line = annotation.line, .isError = true});
}

std::string IncludePayload(const Annotation& annotation)
{
    const std::string name = AnnotationValues::Lower(annotation.name);
    if (name == "include") {
        std::string value = AnnotationValues::NamedValue(annotation, "path");
        if (value.empty()) value = AnnotationValues::NamedValue(annotation, "header");
        if (value.empty()) value = AnnotationValues::PositionalArg(annotation, 0);
        return value;
    }
    return AnnotationValues::NamedValue(annotation, "include");
}

void ValidateExclusive(const Annotation& annotation,
                       const char* first, const char* second,
                       std::vector<GeneratedDiagnostic>& diagnostics)
{
    if (AnnotationValues::FindArg(annotation, first) != nullptr &&
        AnnotationValues::FindArg(annotation, second) != nullptr) {
        AddError(annotation, diagnostics,
                 "include fields " + std::string(first) + "/" + second +
                 " are mutually exclusive");
    }
}

void ValidateList(const Annotation& annotation, const std::string& payload,
                  std::vector<GeneratedDiagnostic>& diagnostics)
{
    const bool includeDirective = AnnotationValues::Lower(annotation.name) == "include";
    if (payload.empty()) {
        if (includeDirective || AnnotationValues::FindArg(annotation, "include")) {
            AddError(annotation, diagnostics, "include requires a non-empty path");
        }
        return;
    }
    std::string list = AnnotationValues::Trim(payload);
    if (list.size() >= 2 && list.front() == '[' && list.back() == ']') {
        list = list.substr(1, list.size() - 2);
    }
    for (const std::string& rawPath : AnnotationText::SplitTopLevel(list)) {
        const std::string path = AnnotationValues::Unquote(rawPath);
        if (!AnnotationText::IsIncludePathSafe(path)) {
            AddError(annotation, diagnostics,
                     "include path is invalid: '" +
                     AnnotationText::SanitizeComment(path) + "'");
        }
    }
}

} // namespace

void ValidateIncludes(const Annotation& annotation,
                      std::vector<GeneratedDiagnostic>& diagnostics)
{
    const std::string name = AnnotationValues::Lower(annotation.name);
    if (name != "include" && name != "cpp" && name != "raw" && name != "code" &&
        name != "vulkan") return;
    if (name == "include") {
        ValidateExclusive(annotation, "path", "header", diagnostics);
    }
    ValidateList(annotation, IncludePayload(annotation), diagnostics);
}

} // namespace ConcordScript::AnnotationValidation
