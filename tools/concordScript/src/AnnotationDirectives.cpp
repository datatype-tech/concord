#include "AnnotationDirectives.h"

#include "AnnotationCodeGen.h"
#include "AnnotationText.h"
#include "AnnotationValues.h"

namespace ConcordScript::AnnotationDirectives {

using AnnotationText::AppendInclude;
using AnnotationText::SanitizeComment;
using AnnotationText::SplitTopLevel;
using AnnotationValues::ArgValue;
using AnnotationValues::DecodeQuoted;
using AnnotationValues::Lower;
using AnnotationValues::NamedValue;
using AnnotationValues::PositionalArg;
using AnnotationValues::Trim;

namespace {

void AppendAnnotationLabel(std::string& destination, const Annotation& annotation,
                           const std::string& prefix, const std::string& owner)
{
    destination += "\n/* ConcordScript";
    if (!prefix.empty()) destination += " " + prefix;
    destination += " @" + SanitizeComment(annotation.name);
    if (annotation.hasArgs) {
        destination += "(" + SanitizeComment(annotation.argsText) + ")";
    }
    destination += " " + SanitizeComment(owner);
}

bool IsTrue(const Annotation& annotation, const std::string& key)
{
    return Lower(NamedValue(annotation, key)) == "true";
}

std::string RawPositional(const Annotation& annotation, size_t index)
{
    size_t current = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (!argument.hasKey && current++ == index) return Trim(argument.value);
    }
    return {};
}

std::string RawNamed(const Annotation& annotation, const std::string& key,
                     size_t positional)
{
    if (const AnnotationArg* argument = AnnotationValues::FindArg(annotation, key)) {
        return Trim(argument->value);
    }
    return RawPositional(annotation, positional);
}

std::string Payload(const Annotation& annotation)
{
    if (annotation.hasBody) return annotation.bodyText;
    std::string value = NamedValue(annotation, "code");
    if (value.empty()) value = NamedValue(annotation, "text");
    if (value.empty()) value = NamedValue(annotation, "source");
    if (value.empty()) value = PositionalArg(annotation, 0);
    return DecodeQuoted(value);
}

} // namespace

void ValidateSection(AnnotationOutput& output, const Annotation& annotation)
{
    const std::string section = Lower(NamedValue(annotation, "section"));
    if (!section.empty() && section != "header" && section != "source") {
        output.diagnostics.push_back(GeneratedDiagnostic{
            .message = "section must be 'header' or 'source'",
            .line = annotation.line,
            .isError = true});
    }
}

void AppendCode(std::string& destination, const Annotation& annotation,
                const std::string& owner)
{
    AppendAnnotationLabel(destination, annotation, "", owner);
    destination += " */\n";
    const bool system = IsTrue(annotation, "system") || IsTrue(annotation, "angle");
    const std::string include = NamedValue(annotation, "include");
    if (!include.empty()) {
        std::string list = include;
        if (list.size() >= 2 && list.front() == '[' && list.back() == ']') {
            list = list.substr(1, list.size() - 2);
        }
        for (const std::string& path : SplitTopLevel(list)) {
            AppendInclude(destination, path, system);
        }
    }
    destination += Payload(annotation);
    if (destination.empty() || destination.back() != '\n') destination += '\n';
}

void AppendMetadata(std::string& destination, const Annotation& annotation,
                    const std::string& owner)
{
    AppendAnnotationLabel(destination, annotation, "metadata", owner);
    if (annotation.hasBody) {
        destination += "\n" + SanitizeComment(annotation.bodyText);
    }
    destination += " */\n";
}

void AppendDirective(std::string& destination, const Annotation& annotation,
                     const std::string& owner)
{
    (void)owner;
    const std::string name = Lower(annotation.name);
    if (name == "include") {
        std::string paths = NamedValue(annotation, "path");
        if (paths.empty()) paths = NamedValue(annotation, "header");
        if (paths.empty()) paths = PositionalArg(annotation, 0);
        if (paths.empty()) return;
        if (paths.size() >= 2 && paths.front() == '[' && paths.back() == ']') {
            paths = paths.substr(1, paths.size() - 2);
        }
        const bool system = IsTrue(annotation, "system") || IsTrue(annotation, "angle");
        for (const std::string& path : SplitTopLevel(paths)) {
            AppendInclude(destination, path, system);
        }
        return;
    }
    if (name == "define") {
        std::string key = ArgValue(annotation, "name", 0);
        std::string value = RawNamed(annotation, "value", 1);
        if (key.empty()) key = PositionalArg(annotation, 0);
        if (value.empty()) value = RawPositional(annotation, 1);
        if (!key.empty()) {
            destination += "#define " + key + (value.empty() ? "" : " " + value) + "\n";
        }
        return;
    }
    if (name == "pragma") {
        std::string value = annotation.bodyText;
        if (Trim(value).empty()) value = ArgValue(annotation, "value", 0);
        if (!Trim(value).empty()) destination += "#pragma " + Trim(value) + "\n";
    }
}

} // namespace ConcordScript::AnnotationDirectives
