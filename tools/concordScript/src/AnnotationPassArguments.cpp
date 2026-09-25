// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "AnnotationPassArguments.h"

#include "AnnotationPassSupport.h"
#include "AnnotationValues.h"

#include <utility>

namespace ConcordScript::AnnotationPass {

namespace {

void SetNamed(PassArguments& values, std::string key, std::string value)
{
    key = AnnotationValues::Lower(AnnotationValues::Trim(std::move(key)));
    if (key == "name") {
        values.name = std::move(value);
        values.nameSpecified = true;
    }
    else if (key == "pass" || key == "callback" || key == "function") {
        values.callback = std::move(value);
        values.callbackSpecified = true;
    } else if (key == "phase") {
        values.phase = std::move(value);
        values.phaseSpecified = true;
    } else if (key == "order") {
        values.order = std::move(value);
        values.orderSpecified = true;
    }
    else if (key == "userdata" || key == "user_data" || key == "data") {
        values.userData = std::move(value);
        values.userDataSpecified = true;
    }
}

bool Occupied(const PassArguments& values, size_t slot)
{
    return (slot == 0 && values.nameSpecified) ||
           (slot == 1 && values.callbackSpecified) ||
           (slot == 2 && values.phaseSpecified) ||
           (slot == 3 && values.orderSpecified) ||
           (slot == 4 && values.userDataSpecified);
}

void SetPositional(PassArguments& values, size_t slot, std::string value)
{
    if (slot == 0) {
        values.name = std::move(value);
        values.nameSpecified = true;
    } else if (slot == 1) {
        values.callback = std::move(value);
        values.callbackSpecified = true;
    } else if (slot == 2) {
        values.phase = std::move(value);
        values.phaseSpecified = true;
    } else if (slot == 3) {
        values.order = std::move(value);
        values.orderSpecified = true;
    } else if (slot == 4) {
        values.userData = std::move(value);
        values.userDataSpecified = true;
    }
}

} // namespace

PassArguments ReadArguments(const Annotation& annotation)
{
    PassArguments values;
    size_t slot = 0;
    for (const AnnotationArg& argument : annotation.args) {
        if (argument.hasKey) {
            SetNamed(values, argument.key, AnnotationValues::Unquote(argument.value));
            continue;
        }
        while (slot < 5 && Occupied(values, slot)) ++slot;
        SetPositional(values, slot, AnnotationValues::Unquote(argument.value));
        if (slot < 5) ++slot;
    }
    return values;
}

bool HasCallbackArgument(const std::vector<AnnotationArg>& arguments)
{
    for (const AnnotationArg& argument : arguments) {
        if (!argument.hasKey) continue;
        const std::string key = AnnotationValues::Lower(
            AnnotationValues::Trim(argument.key));
        if (key == "callback" || key == "function" || key == "pass") return true;
    }
    Annotation annotation;
    annotation.args = arguments;
    const PassArguments values = ReadArguments(annotation);
    if (!values.callbackSpecified || values.callback.empty()) return false;
    const std::string callback = AnnotationValues::Trim(values.callback);
    if (callback == "nullptr" || callback == "0" ||
        !AnnotationPassSupport::NormalizePhase(callback).empty() ||
        AnnotationPassSupport::IsOrderLiteral(callback)) {
        return false;
    }
    // A positional callback is the second slot in the documented compact
    // pass form. Numeric values and phase aliases are ordinary Vulkan payload
    // metadata; everything else is handed to the C++ compiler as an expression.
    return AnnotationPassSupport::IsCallbackExpression(callback);
}

} // namespace ConcordScript::AnnotationPass
