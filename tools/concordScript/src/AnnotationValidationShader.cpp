// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationValidation.h"

#include "AnnotationShaderOptions.h"
#include "AnnotationShaderStages.h"
#include "AnnotationShaderSupport.h"
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

bool IsShaderKey(const std::string& key)
{
    for (const char* name : {"name", "stage", "entry", "language", "path", "output",
                             "file", "source", "code"}) {
        if (key == name) return true;
    }
    return AnnotationShaderOptions::IsOptionKey(key);
}

void ValidateExclusive(const Annotation& annotation,
                       std::initializer_list<const char*> keys, const char* label,
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
                 "shader fields " + std::string(label) + " are mutually exclusive");
    }
}

} // namespace

void ValidateShaderArguments(const Annotation& annotation, const std::string& alias,
                             std::vector<GeneratedDiagnostic>& diagnostics)
{
    ValidateExclusive(annotation, {"path", "output", "file"},
                      "path/output/file", diagnostics);
    ValidateExclusive(annotation, {"source", "code"}, "source/code", diagnostics);
    const AnnotationShaderOptions::Values options = AnnotationShaderOptions::Read(annotation);
    AnnotationShaderOptions::Validate(annotation, options, diagnostics);
    for (const AnnotationArg& argument : annotation.args) {
        if (argument.hasKey && !IsShaderKey(Key(argument))) {
            AddError(annotation, diagnostics,
                     "unknown shader argument '" + Key(argument) + "'");
        }
    }

    std::vector<std::string> positional = AnnotationValues::PositionalValues(annotation);
    bool ambiguous = false;
    const bool hasNamedStage = AnnotationValues::FindArg(annotation, "stage") != nullptr;
    bool positionalStage = false;
    if (alias.empty() && !hasNamedStage) {
        const std::size_t stage = AnnotationShaderSupport::PositionalStageIndex(
            annotation, positional, ambiguous);
        if (stage != std::string::npos) {
            positional.erase(positional.begin() + static_cast<std::ptrdiff_t>(stage));
            positionalStage = true;
        }
    } else if (hasNamedStage || !alias.empty()) {
        std::size_t index = 0;
        for (const AnnotationArg& argument : annotation.args) {
            if (argument.hasKey) continue;
            if (index++ > 1) break;
            if (!argument.quoted && AnnotationShaderStages::IsSupported(
                    AnnotationShaderStages::Normalize(argument.value))) {
                positionalStage = true;
                break;
            }
        }
    }
    if (hasNamedStage && positionalStage) {
        AddError(annotation, diagnostics,
                 "shader stage is specified by both stage= and a positional argument");
    }
    if (!alias.empty() && positionalStage) {
        AddError(annotation, diagnostics,
                 "shader stage alias '@" + annotation.name +
                 "' conflicts with a positional stage argument");
    }

    std::size_t occupied = 0;
    if (AnnotationValues::FindArg(annotation, "name")) ++occupied;
    if (AnnotationValues::FindArg(annotation, "entry")) ++occupied;
    if (AnnotationValues::FindArg(annotation, "language")) ++occupied;
    if (AnnotationValues::FindArg(annotation, "path") ||
        AnnotationValues::FindArg(annotation, "output") ||
        AnnotationValues::FindArg(annotation, "file")) ++occupied;
    const std::size_t maximum = occupied < 4 ? 4 - occupied : 0;
    if (positional.size() > maximum) {
        AddError(annotation, diagnostics,
                 "shader has too many positional arguments (maximum is " +
                 std::to_string(maximum) + " for the named fields)");
    }

    if (!alias.empty()) {
        if (const AnnotationArg* stage = AnnotationValues::FindArg(annotation, "stage")) {
            const std::string explicitStage = AnnotationShaderStages::Normalize(stage->value);
            const std::string aliasStage = AnnotationShaderStages::Normalize(alias);
            if (explicitStage != aliasStage) {
                AddError(annotation, diagnostics,
                         "shader stage alias '@" + annotation.name +
                         "' conflicts with stage='" + explicitStage + "'");
            }
        }
    }
}

} // namespace ConcordScript::AnnotationValidation
