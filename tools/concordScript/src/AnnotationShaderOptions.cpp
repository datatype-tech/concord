// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationShaderOptions.h"

#include "AnnotationText.h"
#include "AnnotationValues.h"

#include <initializer_list>
#include <utility>

namespace ConcordScript::AnnotationShaderOptions {
namespace {

using AnnotationValues::DecodeQuoted;
using AnnotationValues::FindArg;
using AnnotationValues::Lower;
using AnnotationValues::Trim;

bool Matches(const std::string& key, std::initializer_list<const char*> names)
{
    for (const char* name : names) {
        if (key == name) return true;
    }
    return false;
}

const AnnotationArg* FindField(const Annotation& annotation,
                               std::initializer_list<const char*> names)
{
    for (const char* name : names) {
        if (const AnnotationArg* argument = FindArg(annotation, name)) return argument;
    }
    return nullptr;
}

std::vector<std::string> ReadList(const AnnotationArg* argument)
{
    if (!argument) return {};
    std::string text = Trim(argument->value);
    if (text.size() >= 2 &&
        ((text.front() == '[' && text.back() == ']') ||
         (text.front() == '{' && text.back() == '}'))) {
        text = Trim(text.substr(1, text.size() - 2));
        if (text.empty()) return {};
        std::vector<std::string> values;
        for (const std::string& part : AnnotationText::SplitTopLevel(text)) {
            values.push_back(DecodeQuoted(part));
        }
        return values;
    }
    return {DecodeQuoted(text)};
}

void AddError(const Annotation& annotation, std::vector<GeneratedDiagnostic>& diagnostics,
              std::string message)
{
    diagnostics.push_back(GeneratedDiagnostic{
        .message = std::move(message), .line = annotation.line, .isError = true});
}

bool HasInvalidCharacter(const std::string& value)
{
    for (const unsigned char character : value) {
        if (character == 0 || character == '\r' || character == '\n' || character < 0x20) {
            return true;
        }
    }
    return false;
}

void ValidateList(const Annotation& annotation, const std::vector<std::string>& values,
                  const char* label, std::vector<GeneratedDiagnostic>& diagnostics)
{
    for (const std::string& value : values) {
        if (value.empty()) {
            AddError(annotation, diagnostics,
                     std::string("shader ") + label + " entries cannot be empty");
        } else if (HasInvalidCharacter(value)) {
            AddError(annotation, diagnostics,
                     std::string("shader ") + label +
                         " entries cannot contain control characters");
        }
    }
}

void ValidateAliases(const Annotation& annotation,
                     std::initializer_list<const char*> aliases, const char* label,
                     std::vector<GeneratedDiagnostic>& diagnostics)
{
    std::size_t count = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (argument.hasKey && Matches(Lower(Trim(argument.key)), aliases)) ++count;
    }
    if (count > 1) {
        AddError(annotation, diagnostics,
                 std::string("shader ") + label + " fields are mutually exclusive");
    }
}

} // namespace

bool IsOptionKey(const std::string& key)
{
    const std::string normalized = Lower(Trim(key));
    return Matches(normalized, {"defines", "define", "macros", "include_dirs",
                                "include-dirs", "includedirs", "includes", "include",
                                "include_paths", "include-paths", "options", "flags",
                                "compile_options", "compile-options", "compiler_options",
                                "compiler-options", "args", "arguments"});
}

Values Read(const Annotation& annotation)
{
    Values values;
    values.defines = ReadList(FindField(annotation, {"defines", "define", "macros"}));
    values.includeDirs = ReadList(FindField(annotation, {"include_dirs", "include-dirs",
                                                          "includedirs", "includes", "include",
                                                          "include_paths", "include-paths"}));
    values.options = ReadList(FindField(annotation, {"options", "flags", "compile_options",
                                                     "compile-options", "compiler_options",
                                                     "compiler-options", "args", "arguments"}));
    for (std::string& directory : values.includeDirs) directory = AnnotationText::NormalizePath(directory);
    return values;
}

void Validate(const Annotation& annotation, const Values& values,
              std::vector<GeneratedDiagnostic>& diagnostics)
{
    ValidateAliases(annotation, {"defines", "define", "macros"}, "defines", diagnostics);
    ValidateAliases(annotation, {"include_dirs", "include-dirs", "includedirs", "includes",
                                 "include", "include_paths", "include-paths"},
                    "include directory", diagnostics);
    ValidateAliases(annotation, {"options", "flags", "compile_options", "compile-options",
                                 "compiler_options", "compiler-options", "args", "arguments"},
                    "compiler option", diagnostics);
    ValidateList(annotation, values.defines, "defines", diagnostics);
    ValidateList(annotation, values.includeDirs, "include directory", diagnostics);
    ValidateList(annotation, values.options, "compiler option", diagnostics);
}

} // namespace ConcordScript::AnnotationShaderOptions
