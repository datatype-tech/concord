#include "Render.h"

namespace ConcordScript {

std::string RenderEngineInclude(const std::string& path)
{
    std::string headerPath = path;
    if (headerPath.rfind("Concord.", 0) == 0) {
        headerPath.replace(0, 8, "Concord/");
        for (char& character : headerPath) {
            if (character == '.') character = '/';
        }
    }
    return "#include <" + headerPath + ".h>\n";
}

std::string RenderProjectInclude(const std::string& dottedPath)
{
    const size_t firstDot = dottedPath.find('.');
    std::string relativePath = (firstDot == std::string::npos)
        ? dottedPath
        : dottedPath.substr(firstDot + 1);

    for (char& c : relativePath) {
        if (c == '.') {
            c = '/';
        }
    }
    return "#include \"" + relativePath + ".gen.h\"\n";
}

std::string RenderBaseClauses(const ClassDecl& decl)
{
    std::vector<std::string> bases;
    if (!decl.baseClass.empty()) {
        bases.push_back("public " + decl.baseClass);
    }
    for (const std::string& iface : decl.interfaces) {
        bases.push_back("public " + iface);
    }

    if (bases.empty()) {
        return "";
    }

    std::string out = " : ";
    for (size_t i = 0; i < bases.size(); ++i) {
        if (i > 0) {
            out += ", ";
        }
        out += bases[i];
    }
    return out;
}

} // namespace ConcordScript
