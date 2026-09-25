// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_ANNOTATIONSHADEROPTIONS_H
#define CONCORDSCRIPT_ANNOTATIONSHADEROPTIONS_H

#include "Ast.h"
#include "CodeGen.h"

#include <string>
#include <vector>

namespace ConcordScript::AnnotationShaderOptions {

/** Normalized compiler options attached to one shader annotation. */
struct Values {
    std::vector<std::string> defines;
    std::vector<std::string> includeDirs;
    std::vector<std::string> options;
};

/** Reads canonical compiler-option fields and their documented aliases. */
Values Read(const Annotation& annotation);

/** Validates option values before they are emitted into build files. */
void Validate(const Annotation& annotation, const Values& values,
              std::vector<GeneratedDiagnostic>& diagnostics);

/** Returns whether a named argument belongs to the shader option contract. */
bool IsOptionKey(const std::string& key);

} // namespace ConcordScript::AnnotationShaderOptions

#endif // CONCORDSCRIPT_ANNOTATIONSHADEROPTIONS_H
