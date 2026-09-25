// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_ANNOTATIONSHADERSUPPORT_H
#define CONCORDSCRIPT_ANNOTATIONSHADERSUPPORT_H

#include "Ast.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript::AnnotationShaderSupport {

/** Returns whether a shader entry point is a valid source identifier. */
bool IsIdentifier(const std::string& value);

/** Returns whether a language name is safe to place in build metadata. */
bool IsLanguageToken(const std::string& value);

/** Finds a positional stage in the documented first two slots. */
std::size_t PositionalStageIndex(const Annotation& annotation,
                                 const std::vector<std::string>& values,
                                 bool& ambiguous);

} // namespace ConcordScript::AnnotationShaderSupport

#endif // CONCORDSCRIPT_ANNOTATIONSHADERSUPPORT_H
