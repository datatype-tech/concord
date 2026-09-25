#ifndef CONCORDSCRIPT_RENDER_H
#define CONCORDSCRIPT_RENDER_H

#include "Ast.h"

#include <string>

namespace ConcordScript {

/** Renders a Concord engine import as an angle-bracket #include. */
std::string RenderEngineInclude(const std::string& path);

/**
 * Renders `use Project.Relative.Name;` as a quoted #include of the
 * corresponding generated header.
 *
 * The first dotted segment is the project name and names no directory of
 * its own — concordc's output tree is rooted at the project directory, so
 * "MyGame.Player" and a lone "Player" resolve to the same generated header
 * regardless of which project it is written from. Only the segments after
 * the first become path components.
 */
std::string RenderProjectInclude(const std::string& dottedPath);

/** Renders a class's base-class list from ClassDecl's extends/implements fields. */
std::string RenderBaseClauses(const ClassDecl& decl);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_RENDER_H
