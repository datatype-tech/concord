#ifndef CONCORDSCRIPT_ANNOTATIONPASS_H
#define CONCORDSCRIPT_ANNOTATIONPASS_H

#include "Ast.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript {

struct AnnotationOutput;

namespace AnnotationPass {

/** Returns true for the built-in custom Vulkan pass annotation spellings. */
bool IsAlias(const std::string& name);

/** Returns whether arguments explicitly select the positional Vulkan pass form. */
bool HasCallbackArgument(const std::vector<AnnotationArg>& arguments);

/** Validates one pass annotation and appends its registration thunk. */
void Append(AnnotationOutput& output, const Annotation& annotation,
            const std::string& moduleName, const std::string& owner,
            size_t ordinal);

} // namespace AnnotationPass
} // namespace ConcordScript

#endif // CONCORDSCRIPT_ANNOTATIONPASS_H
