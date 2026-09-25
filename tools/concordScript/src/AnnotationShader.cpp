#include "AnnotationShader.h"

#include "AnnotationShaderOptions.h"
#include "AnnotationShaderStages.h"
#include "AnnotationShaderSupport.h"
#include "AnnotationText.h"
#include "AnnotationValues.h"

#include <cstddef>
#include <utility>

namespace ConcordScript::AnnotationShader {

using AnnotationText::IsSafePath;
using AnnotationText::NormalizePath;
using AnnotationText::SafePart;
using AnnotationShaderStages::IsSupported;
using AnnotationShaderStages::Normalize;
using AnnotationValues::DecodeQuoted;
using AnnotationValues::NamedValue;
using AnnotationValues::PositionalValues;

void AddError(const Annotation& annotation, std::vector<GeneratedDiagnostic>& diagnostics,
              std::string message)
{
    diagnostics.push_back(GeneratedDiagnostic{
        .message = std::move(message), .line = annotation.line, .isError = true});
}

using AnnotationShaderSupport::IsIdentifier;
using AnnotationShaderSupport::IsLanguageToken;
using AnnotationShaderSupport::PositionalStageIndex;

Result Build(const Annotation& annotation, const std::string& moduleName,
             const std::string& owner, size_t ordinal, const std::string& alias)
{
    std::vector<std::string> positional = PositionalValues(annotation);
    std::string stage = NamedValue(annotation, "stage");
    if (stage.empty()) stage = alias;
    bool ambiguous = false;
    if (stage.empty()) {
        const size_t stageIndex = PositionalStageIndex(annotation, positional, ambiguous);
        if (stageIndex != std::string::npos) {
            stage = positional[stageIndex];
            positional.erase(positional.begin() + static_cast<std::ptrdiff_t>(stageIndex));
        }
    }
    if (stage.empty()) stage = "frag";
    size_t cursor = 0;
    std::string name = NamedValue(annotation, "name");
    if (name.empty() && cursor < positional.size()) name = positional[cursor++];
    if (name.empty()) name = moduleName + "_shader_" + std::to_string(ordinal);

    Result result;
    result.shader.name = SafePart(name, moduleName + "_shader_" + std::to_string(ordinal));
    result.shader.stage = SafePart(Normalize(stage), "frag");
    result.shader.entry = NamedValue(annotation, "entry");
    if (result.shader.entry.empty() && cursor < positional.size()) result.shader.entry = positional[cursor++];
    if (result.shader.entry.empty()) result.shader.entry = "main";
    result.shader.language = AnnotationValues::Lower(NamedValue(annotation, "language"));
    if (result.shader.language.empty() && cursor < positional.size()) result.shader.language = AnnotationValues::Lower(positional[cursor++]);
    if (result.shader.language.empty()) result.shader.language = "glsl";
    const AnnotationShaderOptions::Values options = AnnotationShaderOptions::Read(annotation);
    result.shader.defines = options.defines;
    result.shader.includeDirs = options.includeDirs;
    result.shader.options = options.options;
    result.shader.owner = owner;
    result.shader.line = annotation.line;
    result.ambiguousPositional = ambiguous;

    result.explicitPath = NamedValue(annotation, "path");
    if (result.explicitPath.empty()) result.explicitPath = NamedValue(annotation, "output");
    if (result.explicitPath.empty()) result.explicitPath = NamedValue(annotation, "file");
    if (result.explicitPath.empty() && cursor < positional.size()) result.explicitPath = positional[cursor++];
    result.explicitPath = NormalizePath(result.explicitPath);
    result.shader.relativePath = result.explicitPath.empty()
        ? "shaders/" + SafePart(moduleName, "module") + "." + result.shader.name + "." + result.shader.stage
        : result.explicitPath;
    if (result.explicitPath.empty() && result.shader.language != "glsl") {
        result.shader.relativePath += "." + SafePart(result.shader.language, "shader");
    }
    result.shader.external = !annotation.hasBody && !result.explicitPath.empty();
    result.inlineSource = DecodeQuoted(NamedValue(annotation, "source"));
    if (result.inlineSource.empty()) result.inlineSource = DecodeQuoted(NamedValue(annotation, "code"));
    if (!result.inlineSource.empty()) result.shader.external = false;
    return result;
}

void Validate(const Annotation& annotation, const Result& result,
              std::vector<GeneratedDiagnostic>& diagnostics)
{
    if (result.ambiguousPositional) {
        AddError(annotation, diagnostics,
                 "ambiguous shader positional arguments; use explicit name= and stage=");
    }
    if (!IsSupported(result.shader.stage)) {
        AddError(annotation, diagnostics, "unsupported shader stage '" + result.shader.stage + "'");
    }
    if (!IsIdentifier(result.shader.entry)) {
        AddError(annotation, diagnostics, "shader entry must be a valid identifier");
    }
    if (!IsLanguageToken(result.shader.language)) {
        AddError(annotation, diagnostics, "shader language must be a safe token");
    }
    if (!result.explicitPath.empty() && !IsSafePath(result.explicitPath)) {
        AddError(annotation, diagnostics, "shader path must be relative and cannot contain '..'");
    }
    if (!annotation.hasBody && !result.shader.external && result.inlineSource.empty()) {
        AddError(annotation, diagnostics, "shader annotation needs a body or file/path argument");
    }
}

} // namespace ConcordScript::AnnotationShader
