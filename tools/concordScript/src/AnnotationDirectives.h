#ifndef CONCORDSCRIPT_ANNOTATIONDIRECTIVES_H
#define CONCORDSCRIPT_ANNOTATIONDIRECTIVES_H

#include "Ast.h"

#include <string>

namespace ConcordScript {

struct AnnotationOutput;

namespace AnnotationDirectives {

/** Validates the optional output section on a code or directive annotation. */
void ValidateSection(AnnotationOutput& output, const Annotation& annotation);

/** Emits a C++/Vulkan escape-hatch annotation into a generated stream. */
void AppendCode(std::string& destination, const Annotation& annotation,
                const std::string& owner);

/** Emits an unknown annotation as a safe, source-visible metadata comment. */
void AppendMetadata(std::string& destination, const Annotation& annotation,
                    const std::string& owner);

/** Emits include, define, or pragma directives into a generated stream. */
void AppendDirective(std::string& destination, const Annotation& annotation,
                     const std::string& owner);

} // namespace AnnotationDirectives
} // namespace ConcordScript

#endif // CONCORDSCRIPT_ANNOTATIONDIRECTIVES_H
