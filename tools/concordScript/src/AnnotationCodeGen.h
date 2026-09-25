#ifndef CONCORDSCRIPT_ANNOTATIONCODEGEN_H
#define CONCORDSCRIPT_ANNOTATIONCODEGEN_H

#include "Ast.h"
#include "CodeGen.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript {

/** Selects the generated C++ stream receiving raw annotation code. */
enum class AnnotationPlacement { Header, Source };

/** Intermediate output produced while lowering a group of annotations. */
struct AnnotationOutput {
    std::string headerText;
    std::string sourceText;
    std::string tailSourceText;
    std::vector<GeneratedSidecar> sidecars;
    std::vector<GeneratedShader> shaders;
    std::vector<GeneratedAnnotation> records;
    std::vector<GeneratedDiagnostic> diagnostics;
    bool requiresRenderApi = false;
    bool usesVulkan = false;
};

/**
 * Lowers annotations while retaining unknown directives in generated C++.
 * Shader blocks become sidecar source files and metadata records; `cpp` and
 * `vulkan` blocks are copied verbatim to the selected C++ stream.
 */
AnnotationOutput RenderAnnotations(const std::vector<Annotation>& annotations,
                                   const std::string& moduleName,
                                   const std::string& owner,
                                   AnnotationPlacement placement,
                                   size_t& shaderOrdinal);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_ANNOTATIONCODEGEN_H
