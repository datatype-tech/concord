// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_ANNOTATIONPASSARGUMENTS_H
#define CONCORDSCRIPT_ANNOTATIONPASSARGUMENTS_H

#include "Ast.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript::AnnotationPass {

/** Normalized positional and named values accepted by a Vulkan pass. */
struct PassArguments {
    std::string name;
    std::string callback;
    std::string phase;
    std::string order;
    std::string userData;
    bool nameSpecified = false;
    bool callbackSpecified = false;
    bool phaseSpecified = false;
    bool orderSpecified = false;
    bool userDataSpecified = false;
};

/** Reads the documented pass argument order while honoring named values. */
PassArguments ReadArguments(const Annotation& annotation);

/** Returns whether an annotation contains a callback selector. */
bool HasCallbackArgument(const std::vector<AnnotationArg>& arguments);

} // namespace ConcordScript::AnnotationPass

#endif // CONCORDSCRIPT_ANNOTATIONPASSARGUMENTS_H
