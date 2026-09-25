// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationValidation.h"

#include "AnnotationPass.h"
#include "AnnotationShaderStages.h"
#include "AnnotationValues.h"

#include <string>

namespace ConcordScript::AnnotationValidation {

void ValidateBuiltIn(const Annotation& annotation, const std::string& alias,
                     std::vector<GeneratedDiagnostic>& diagnostics)
{
    const std::string name = AnnotationValues::Lower(annotation.name);
    if (name == "shader" || AnnotationShaderStages::IsAlias(name)) {
        ValidateShaderArguments(annotation, alias, diagnostics);
    } else if (AnnotationPass::IsAlias(name)) {
        ValidatePassArguments(annotation, false, diagnostics);
    }
}

} // namespace ConcordScript::AnnotationValidation
