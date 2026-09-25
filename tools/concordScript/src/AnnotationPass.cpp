#include "AnnotationPass.h"

#include "AnnotationCodeGen.h"
#include "AnnotationPassArguments.h"
#include "AnnotationPassSupport.h"
#include "AnnotationText.h"
#include "AnnotationValues.h"

#include <string>
#include <utility>

namespace ConcordScript::AnnotationPass {

using AnnotationPassSupport::CppString;
using AnnotationPassSupport::IsCallbackExpression;
using AnnotationPassSupport::IsOrderLiteral;
using AnnotationPassSupport::IsUserDataExpression;
using AnnotationPassSupport::NormalizePhase;
using AnnotationText::SafePart;
using AnnotationValues::Lower;
using AnnotationValues::Trim;

namespace {

void Error(AnnotationOutput& output, const Annotation& annotation, std::string message)
{
    output.diagnostics.push_back(GeneratedDiagnostic{
        .message = std::move(message), .line = annotation.line, .isError = true});
}

} // namespace

bool IsAlias(const std::string& value)
{
    const std::string name = Lower(value);
    return name == "pass" || name == "vulkan_pass" || name == "vulkan-pass" ||
           name == "render_pass" || name == "render-pass" || name == "custom_pass" ||
           name == "custom-pass" || name == "render.pass";
}

void Append(AnnotationOutput& output, const Annotation& annotation,
            const std::string& moduleName, const std::string& owner, size_t ordinal)
{
    const PassArguments arguments = ReadArguments(annotation);
    const std::string name = arguments.name.empty()
        ? moduleName + "_pass_" + std::to_string(ordinal) : arguments.name;
    std::string callback = Trim(arguments.callback);
    const std::string phase = Trim(arguments.phase);
    const std::string order = Trim(arguments.order);
    const std::string userData = Trim(arguments.userData);

    const std::string symbol = "ConcordScriptPass_" + SafePart(moduleName, "module") +
                               "_" + std::to_string(ordinal);
    std::string callbackBody;
    if (callback.empty() && annotation.hasBody) {
        callback = symbol + "Callback";
        callbackBody = "\nnamespace {\nbool " + callback +
                       "(const ::Concord::VulkanPassContext& context, void* userData)\n{\n"
                       "    (void)context;\n    (void)userData;\n" + annotation.bodyText;
        // Always provide a fallback return: an inline body may return only on
        // one branch, and a bool callback must still be total at the ABI edge.
        callbackBody += "\n    return true;\n}\n}\n";
    }
    if (callback.empty() || !IsCallbackExpression(callback)) {
        Error(output, annotation, "pass requires callback=<non-empty C++ expression>");
        return;
    }
    const std::string phaseName = NormalizePhase(phase.empty() ? "after_scene" : phase);
    if (phaseName.empty()) {
        Error(output, annotation,
              "pass phase must be initialize, before_scene, after_scene, or shutdown");
        return;
    }
    if (!order.empty() && !IsOrderLiteral(order)) {
        Error(output, annotation, "pass order must be an unsigned integer literal");
        return;
    }
    if (!userData.empty() && !IsUserDataExpression(userData)) {
        Error(output, annotation, "pass userData must be nullptr, 0, or an identifier");
        return;
    }
    output.requiresRenderApi = true;
    output.usesVulkan = true;
    // A function name decays to VulkanPassCallback, while a caller-owned
    // function-pointer variable is already the correct value.  Preserve the
    // expression exactly so both forms remain usable by the escape hatch.
    const std::string callbackExpression = Trim(callback);
    output.tailSourceText += callbackBody + "\nnamespace {\n";
    output.tailSourceText += "[[maybe_unused]] const bool " + symbol + " =\n";
    output.tailSourceText += "    ::Concord::RegisterVulkanPass(::Concord::VulkanPassDesc{\n";
    output.tailSourceText += "        .name = " + CppString(name) + ",\n";
    output.tailSourceText += "        .phase = ::Concord::VulkanPassPhase::" + phaseName + ",\n";
    output.tailSourceText += "        .order = " + (order.empty() ? "0u" : Trim(order)) + ",\n";
    output.tailSourceText += "        .callback = (" + callbackExpression + "),\n";
    output.tailSourceText += "        .userData = " + (userData.empty() ? "nullptr" : Trim(userData)) +
                             "});\n}\n";
    (void)owner;
}

} // namespace ConcordScript::AnnotationPass
