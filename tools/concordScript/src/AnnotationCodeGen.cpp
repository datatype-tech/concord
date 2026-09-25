#include "AnnotationCodeGen.h"

#include "AnnotationDirectives.h"
#include "AnnotationPass.h"
#include "AnnotationValidation.h"
#include "AnnotationShader.h"
#include "AnnotationShaderStages.h"
#include "AnnotationValues.h"

namespace ConcordScript {

namespace {

/** Removes blank line breaks introduced by the annotation body delimiter. */
std::string StripLeadingLineBreaks(std::string source)
{
    size_t offset = 0;
    while (offset < source.size()) {
        if (source[offset] == '\r') {
            ++offset;
            if (offset < source.size() && source[offset] == '\n') ++offset;
        } else if (source[offset] == '\n') {
            ++offset;
        } else {
            break;
        }
    }
    source.erase(0, offset);
    return source;
}

void RecordAnnotation(AnnotationOutput& output, const Annotation& annotation,
                      const std::string& owner)
{
    output.records.push_back(GeneratedAnnotation{
        .name = annotation.name,
        .arguments = annotation.argsText,
        .body = annotation.bodyText,
        .owner = owner,
        .line = annotation.line,
        .parsedArguments = annotation.args});
}

void RenderShader(AnnotationOutput& output, const Annotation& annotation,
                  const std::string& moduleName, const std::string& owner,
                  const std::string& alias, size_t& ordinal)
{
    Annotation shaderAnnotation = annotation;
    if (!alias.empty() && !AnnotationValues::FindArg(annotation, "stage")) {
        shaderAnnotation.args.push_back(AnnotationArg{
            .key = "stage", .value = alias, .raw = alias, .hasKey = true});
    }
    const AnnotationShader::Result result = AnnotationShader::Build(
        shaderAnnotation, moduleName, owner, ordinal++, alias);
    AnnotationShader::Validate(annotation, result, output.diagnostics);
    if (!result.shader.external) {
        output.sidecars.push_back(GeneratedSidecar{
            .relativePath = result.shader.relativePath,
            .text = StripLeadingLineBreaks(annotation.hasBody ? annotation.bodyText
                                                              : result.inlineSource),
            .kind = "shader"});
    }
    output.shaders.push_back(result.shader);
}

void RenderOne(AnnotationOutput& output, const Annotation& annotation,
               const std::string& moduleName, const std::string& owner,
               AnnotationPlacement placement, size_t& ordinal)
{
    const std::string name = AnnotationValues::Lower(annotation.name);
    AnnotationValidation::ValidateBuiltIn(
        annotation, AnnotationShaderStages::IsAlias(name) ? name : "", output.diagnostics);
    AnnotationValidation::ValidateIncludes(annotation, output.diagnostics);
    if (name == "shader" || AnnotationShaderStages::IsAlias(name)) {
        RenderShader(output, annotation, moduleName, owner,
                     AnnotationShaderStages::IsAlias(name) ? name : "", ordinal);
        return;
    }
    if (AnnotationPass::IsAlias(name)) {
        AnnotationPass::Append(output, annotation, moduleName, owner, ordinal++);
        return;
    }
    if (name == "cpp" || name == "vulkan" || name == "raw" || name == "code") {
        output.usesVulkan = output.usesVulkan || name == "vulkan";
        if (name == "vulkan" && AnnotationPass::HasCallbackArgument(annotation.args)) {
            AnnotationValidation::ValidatePassArguments(annotation, true, output.diagnostics);
        }
        AnnotationDirectives::ValidateSection(output, annotation);
        const std::string section = AnnotationValues::Lower(
            AnnotationValues::ArgValue(annotation, "section", 0));
        const bool toHeader = section == "header" ||
            (section.empty() && name != "vulkan" && placement == AnnotationPlacement::Header);
        AnnotationDirectives::AppendCode(toHeader ? output.headerText : output.sourceText,
                                         annotation, owner);
        if (name == "vulkan" && AnnotationPass::HasCallbackArgument(annotation.args)) {
            AnnotationPass::Append(output, annotation, moduleName, owner, ordinal++);
        }
        return;
    }
    if (name == "include" || name == "define" || name == "pragma") {
        AnnotationDirectives::ValidateSection(output, annotation);
        const std::string section = AnnotationValues::Lower(
            AnnotationValues::NamedValue(annotation, "section"));
        const bool toHeader = section == "header" ||
            (section.empty() && placement == AnnotationPlacement::Header);
        AnnotationDirectives::AppendDirective(toHeader ? output.headerText : output.sourceText,
                                               annotation, owner);
        return;
    }
    const bool toHeader = placement == AnnotationPlacement::Header;
    AnnotationDirectives::AppendMetadata(toHeader ? output.headerText : output.sourceText,
                                         annotation, owner);
}

} // namespace

AnnotationOutput RenderAnnotations(const std::vector<Annotation>& annotations,
                                   const std::string& moduleName,
                                   const std::string& owner,
                                   AnnotationPlacement placement,
                                   size_t& shaderOrdinal)
{
    AnnotationOutput output;
    for (const Annotation& annotation : annotations) {
        RecordAnnotation(output, annotation, owner);
        RenderOne(output, annotation, moduleName, owner, placement, shaderOrdinal);
    }
    return output;
}

} // namespace ConcordScript
