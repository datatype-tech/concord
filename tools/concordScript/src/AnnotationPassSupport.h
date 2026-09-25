#ifndef CONCORDSCRIPT_ANNOTATIONPASSSUPPORT_H
#define CONCORDSCRIPT_ANNOTATIONPASSSUPPORT_H

#include <string>

namespace ConcordScript::AnnotationPassSupport {

/** Escapes a user-provided name into a C++ string literal. */
std::string CppString(std::string value);

/** Returns whether value is a qualified function or object identifier. */
bool IsIdentifierExpression(const std::string& value);

/** Returns whether value can be emitted as a caller-owned C++ expression. */
bool IsCallbackExpression(const std::string& value);

/** Returns whether value is safe as a pass user-data expression. */
bool IsUserDataExpression(const std::string& value);

/** Returns whether value is an unsigned integer literal accepted for order. */
bool IsOrderLiteral(const std::string& value);

/** Normalizes a pass phase alias to the public enum spelling. */
std::string NormalizePhase(std::string value);

} // namespace ConcordScript::AnnotationPassSupport

#endif // CONCORDSCRIPT_ANNOTATIONPASSSUPPORT_H
