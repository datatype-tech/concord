#ifndef CONCORDSCRIPT_ANNOTATIONSHADERSTAGES_H
#define CONCORDSCRIPT_ANNOTATIONSHADERSTAGES_H

#include <string>

namespace ConcordScript::AnnotationShaderStages {

/** Returns true when name is a user-facing shader-stage annotation alias. */
bool IsAlias(const std::string& name);

/** Converts a user-facing stage alias into a Vulkan shader suffix. */
std::string Normalize(std::string stage);

/** Returns whether stage is supported by the shader build integration. */
bool IsSupported(const std::string& stage);

} // namespace ConcordScript::AnnotationShaderStages

#endif // CONCORDSCRIPT_ANNOTATIONSHADERSTAGES_H
