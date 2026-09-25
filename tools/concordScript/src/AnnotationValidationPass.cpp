// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationValidation.h"

#include "AnnotationValues.h"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>

namespace ConcordScript::AnnotationValidation {
namespace {

void AddError(const Annotation& annotation,
              std::vector<GeneratedDiagnostic>& diagnostics, std::string message)
{
    diagnostics.push_back(GeneratedDiagnostic{
        .message = std::move(message), .line = annotation.line, .isError = true});
}

std::string Key(const AnnotationArg& argument)
{
    return AnnotationValues::Lower(AnnotationValues::Trim(argument.key));
}

bool IsPassKey(const std::string& key, bool vulkanEscape)
{
    for (const char* name : {"name", "callback", "function", "pass", "phase", "order",
                             "userdata", "user_data", "data"}) {
        if (key == name) return true;
    }
    if (!vulkanEscape) return false;
    for (const char* name : {"section", "include", "system", "angle", "source", "code",
                             "text"}) {
        if (key == name) return true;
    }
    return false;
}

void ValidateExclusive(const Annotation& annotation,
                       std::initializer_list<const char*> keys,
                       const char* label,
                       std::vector<GeneratedDiagnostic>& diagnostics)
{
    std::size_t count = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey) continue;
        const std::string key = Key(argument);
        for (const char* candidate : keys) {
            if (key == candidate) {
                ++count;
                break;
            }
        }
    }
    if (count > 1) {
        AddError(annotation, diagnostics,
                 "pass fields " + std::string(label) + " are mutually exclusive");
    }
}

} // namespace

void ValidatePassArguments(const Annotation& annotation, bool vulkanEscape,
                           std::vector<GeneratedDiagnostic>& diagnostics)
{
    ValidateExclusive(annotation, {"callback", "function", "pass"},
                      "callback/function/pass", diagnostics);
    ValidateExclusive(annotation, {"userdata", "user_data", "data"},
                      "userData aliases", diagnostics);
    for (const AnnotationArg& argument : annotation.args) {
        if (argument.hasKey && !IsPassKey(Key(argument), vulkanEscape)) {
            AddError(annotation, diagnostics,
                     "unknown pass argument '" + Key(argument) + "'");
        }
    }
    std::size_t occupied = AnnotationValues::FindArg(annotation, "name") ? 1u : 0u;
    if (AnnotationValues::FindArg(annotation, "callback") ||
        AnnotationValues::FindArg(annotation, "function") ||
        AnnotationValues::FindArg(annotation, "pass")) ++occupied;
    if (AnnotationValues::FindArg(annotation, "phase")) ++occupied;
    if (AnnotationValues::FindArg(annotation, "order")) ++occupied;
    if (AnnotationValues::FindArg(annotation, "userdata") ||
        AnnotationValues::FindArg(annotation, "user_data") ||
        AnnotationValues::FindArg(annotation, "data")) ++occupied;
    std::size_t positional = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey) ++positional;
    }
    const std::size_t maximum = occupied < 5 ? 5 - occupied : 0;
    if (positional > maximum) {
        AddError(annotation, diagnostics,
                 "pass has too many positional arguments (maximum is " +
                 std::to_string(maximum) + " for the named fields)");
    }
}

} // namespace ConcordScript::AnnotationValidation
