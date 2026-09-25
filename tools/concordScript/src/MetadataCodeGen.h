#ifndef CONCORDSCRIPT_METADATACODEGEN_H
#define CONCORDSCRIPT_METADATACODEGEN_H

#include "CodeGen.h"

#include <string>
#include <vector>

namespace ConcordScript {

/** Metadata belonging to one generated ConcordScript module. */
struct MetadataModule {
    /** Module path relative to the project root. */
    std::string module;

    /** Annotations in source order. */
    std::vector<GeneratedAnnotation> annotations;

    /** Shaders in source order. */
    std::vector<GeneratedShader> shaders;
};

/** Renders a self-contained C++ metadata registry header. */
std::string RenderMetadataHeader(const std::vector<MetadataModule>& modules);

/** Renders the CMake integration fragment appended to the shader helper. */
std::string RenderMetadataCmake(const std::vector<MetadataModule>& modules);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_METADATACODEGEN_H
