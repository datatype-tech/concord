// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_ANNOTATIONVALIDATION_H
#define CONCORDSCRIPT_ANNOTATIONVALIDATION_H

#include "Ast.h"
#include "CodeGen.h"

#include <string>
#include <vector>

namespace ConcordScript::AnnotationValidation {

/** Validates the argument contract of one built-in shader or pass annotation. */
void ValidateBuiltIn(const Annotation& annotation, const std::string& alias,
                     std::vector<GeneratedDiagnostic>& diagnostics);

/** Validates shader-specific named and positional arguments. */
void ValidateShaderArguments(const Annotation& annotation, const std::string& alias,
                             std::vector<GeneratedDiagnostic>& diagnostics);

/** Validates Vulkan pass-specific named and positional arguments. */
void ValidatePassArguments(const Annotation& annotation, bool vulkanEscape,
                           std::vector<GeneratedDiagnostic>& diagnostics);

/** Reports include paths that cannot be emitted as C++ include directives. */
void ValidateIncludes(const Annotation& annotation,
                      std::vector<GeneratedDiagnostic>& diagnostics);

} // namespace ConcordScript::AnnotationValidation

#endif // CONCORDSCRIPT_ANNOTATIONVALIDATION_H
