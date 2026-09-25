#include "MetadataCodeGen.h"

#include <sstream>

namespace ConcordScript {
namespace {

std::string EscapeCmake(std::string value)
{
    std::string escaped;
    for (const char character : value) {
        if (character == '\\' || character == '"' || character == ';' || character == '$') {
            escaped += '\\';
        }
        escaped += character;
    }
    return escaped;
}

} // namespace

std::string RenderMetadataCmake(const std::vector<MetadataModule>& modules)
{
    std::ostringstream out;
    out << "if(NOT DEFINED CONCORDSCRIPT_GENERATED_ROOT)\n"
        << "  set(CONCORDSCRIPT_GENERATED_ROOT \"${CMAKE_CURRENT_LIST_DIR}\")\n"
        << "endif()\n";
    out << "set(CONCORDSCRIPT_METADATA_HEADER\n"
        << "  \"${CONCORDSCRIPT_GENERATED_ROOT}/ConcordScriptMetadata.gen.h\"\n)\n"
        << "set(CONCORDSCRIPT_ANNOTATION_MANIFESTS\n";
    for (const MetadataModule& module : modules) {
        if (module.annotations.empty()) continue;
        out << "  \"${CONCORDSCRIPT_GENERATED_ROOT}/" << EscapeCmake(module.module)
            << ".annotations.json\"\n";
    }
    out << ")\n"
        << "function(concordscript_attach_metadata target)\n"
        << "  if(NOT EXISTS \"${CONCORDSCRIPT_METADATA_HEADER}\")\n"
        << "    message(FATAL_ERROR \"ConcordScript metadata header is missing\")\n"
        << "  endif()\n"
        << "  target_include_directories(${target} PRIVATE \"${CONCORDSCRIPT_GENERATED_ROOT}\")\n"
        << "  target_sources(${target} PRIVATE \"${CONCORDSCRIPT_METADATA_HEADER}\")\n"
        << "endfunction()\n";
    return out.str();
}

} // namespace ConcordScript
