#ifndef CONCORDSCRIPT_METADATASHADER_H
#define CONCORDSCRIPT_METADATASHADER_H

#include "CodeGen.h"

#include <sstream>

namespace ConcordScript {
namespace MetadataShaderDetail {

inline void Escape(std::ostringstream& out, const std::string& value)
{
    out << '"';
    for (const unsigned char character : value) {
        if (character == '\\' || character == '"') out << '\\' << character;
        else if (character == '\n') out << "\\n";
        else if (character == '\r') out << "\\r";
        else if (character == '\t') out << "\\t";
        else if (character < 0x20) out << "\\x00";
        else out << static_cast<char>(character);
    }
    out << '"';
}

inline void StringTable(std::ostringstream& out, const char* name,
                        const std::vector<std::string>& values)
{
    out << "inline constexpr std::array<std::string_view, " << values.size() << "> "
        << name << "{{\n";
    for (const std::string& value : values) {
        out << "    ";
        Escape(out, value);
        out << ",\n";
    }
    out << "}};\n\n";
}

} // namespace MetadataShaderDetail

/** Renders shader records and flattened compiler-option tables. */
inline std::string RenderShaderMetadata(const std::vector<MetadataModule>& modules)
{
    std::size_t shaderCount = 0;
    std::vector<std::string> defines;
    std::vector<std::string> includeDirs;
    std::vector<std::string> options;
    for (const MetadataModule& module : modules) {
        for (const GeneratedShader& shader : module.shaders) {
            ++shaderCount;
            defines.insert(defines.end(), shader.defines.begin(), shader.defines.end());
            includeDirs.insert(includeDirs.end(), shader.includeDirs.begin(), shader.includeDirs.end());
            options.insert(options.end(), shader.options.begin(), shader.options.end());
        }
    }
    std::ostringstream out;
    MetadataShaderDetail::StringTable(out, "DefineTable", defines);
    MetadataShaderDetail::StringTable(out, "IncludeDirTable", includeDirs);
    MetadataShaderDetail::StringTable(out, "OptionTable", options);
    out << "inline constexpr std::array<ShaderRecord, " << shaderCount
        << "> ShaderTable{{\n";
    std::size_t defineOffset = 0, includeOffset = 0, optionOffset = 0;
    for (const MetadataModule& module : modules) {
        for (const GeneratedShader& shader : module.shaders) {
            out << "    {";
            MetadataShaderDetail::Escape(out, module.module); out << ", ";
            MetadataShaderDetail::Escape(out, shader.name); out << ", ";
            MetadataShaderDetail::Escape(out, shader.stage); out << ", ";
            MetadataShaderDetail::Escape(out, shader.entry); out << ", ";
            MetadataShaderDetail::Escape(out, shader.language); out << ", ";
            MetadataShaderDetail::Escape(out, shader.relativePath); out << ", ";
            out << (shader.external ? "true, " : "false, ");
            MetadataShaderDetail::Escape(out, shader.owner); out << ", " << shader.line << ", "
                << defineOffset << ", " << shader.defines.size() << ", "
                << includeOffset << ", " << shader.includeDirs.size() << ", "
                << optionOffset << ", " << shader.options.size() << "},\n";
            defineOffset += shader.defines.size();
            includeOffset += shader.includeDirs.size();
            optionOffset += shader.options.size();
        }
    }
    out << "}};\n\n"
        << "inline constexpr std::span<const std::string_view> Defines(const ShaderRecord& r) noexcept "
        << "{ return std::span<const std::string_view>(DefineTable).subspan(r.firstDefines, r.defineCount); }\n"
        << "inline constexpr std::span<const std::string_view> IncludeDirs(const ShaderRecord& r) noexcept "
        << "{ return std::span<const std::string_view>(IncludeDirTable).subspan(r.firstIncludeDirs, r.includeDirCount); }\n"
        << "inline constexpr std::span<const std::string_view> Options(const ShaderRecord& r) noexcept "
        << "{ return std::span<const std::string_view>(OptionTable).subspan(r.firstOptions, r.optionCount); }\n"
        << "inline constexpr std::span<const ShaderRecord> Shaders() noexcept { return ShaderTable; }\n\n";
    return out.str();
}

} // namespace ConcordScript

#endif // CONCORDSCRIPT_METADATASHADER_H
