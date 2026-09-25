#ifndef CONCORDSCRIPT_ANNOTATIONSHADER_H
#define CONCORDSCRIPT_ANNOTATIONSHADER_H

#include "Ast.h"
#include "CodeGen.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript::AnnotationShader {

/** Intermediate shader data used by the annotation lowering pass. */
struct Result {
    GeneratedShader shader;
    std::string explicitPath;
    std::string inlineSource;
    /** True when the first two positional arguments both look like stages. */
    bool ambiguousPositional = false;
};

/** Builds normalized shader metadata from one annotation. */
Result Build(const Annotation& annotation, const std::string& moduleName,
             const std::string& owner, size_t ordinal, const std::string& alias);

/** Appends source-located shader validation errors to diagnostics. */
void Validate(const Annotation& annotation, const Result& result,
              std::vector<GeneratedDiagnostic>& diagnostics);

} // namespace ConcordScript::AnnotationShader

#endif // CONCORDSCRIPT_ANNOTATIONSHADER_H
