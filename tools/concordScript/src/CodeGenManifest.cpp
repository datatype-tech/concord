#include "CodeGen.h"

#include <sstream>

namespace ConcordScript {
namespace {

void EscapeJson(std::ostringstream& out, const std::string& text)
{
    out << '"';
    for (const unsigned char character : text) {
        switch (character) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (character < 0x20) {
                out << "\\u00" << "0123456789abcdef"[(character >> 4) & 0xf]
                    << "0123456789abcdef"[character & 0xf];
            } else {
                out << static_cast<char>(character);
            }
        }
    }
    out << '"';
}

void RenderStringArray(std::ostringstream& out, const std::vector<std::string>& values)
{
    out << '[';
    for (size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out << ',';
        EscapeJson(out, values[index]);
    }
    out << ']';
}

void RenderShader(std::ostringstream& out, const GeneratedShader& shader, bool& first)
{
    if (!first) out << ',';
    first = false;
    out << "\n    {\"name\": "; EscapeJson(out, shader.name);
    out << ", \"stage\": "; EscapeJson(out, shader.stage);
    out << ", \"entry\": "; EscapeJson(out, shader.entry);
    out << ", \"language\": "; EscapeJson(out, shader.language);
    out << ", \"defines\": "; RenderStringArray(out, shader.defines);
    out << ", \"includeDirs\": "; RenderStringArray(out, shader.includeDirs);
    out << ", \"options\": "; RenderStringArray(out, shader.options);
    out << ", \"path\": "; EscapeJson(out, shader.relativePath);
    out << ", \"external\": " << (shader.external ? "true" : "false");
    out << ", \"owner\": "; EscapeJson(out, shader.owner);
    out << ", \"line\": " << shader.line << '}';
}

void RenderAnnotation(std::ostringstream& out, const GeneratedAnnotation& annotation,
                      bool& first)
{
    if (!first) out << ',';
    first = false;
    out << "\n    {\"name\": "; EscapeJson(out, annotation.name);
    out << ", \"args\": "; EscapeJson(out, annotation.arguments);
    out << ", \"body\": "; EscapeJson(out, annotation.body);
    out << ", \"owner\": "; EscapeJson(out, annotation.owner);
    out << ", \"line\": " << annotation.line << ", \"parameters\": [";
    for (size_t index = 0; index < annotation.parsedArguments.size(); ++index) {
        const AnnotationArg& parameter = annotation.parsedArguments[index];
        if (index != 0) out << ',';
        out << "{\"key\": "; EscapeJson(out, parameter.key);
        out << ", \"value\": "; EscapeJson(out, parameter.value);
        out << ", \"raw\": "; EscapeJson(out, parameter.raw);
        out << ", \"named\": " << (parameter.hasKey ? "true" : "false");
        out << ", \"quoted\": " << (parameter.quoted ? "true" : "false") << '}';
    }
    out << "]}";
}

} // namespace

std::string RenderAnnotationManifest(const GeneratedFile& file)
{
    std::ostringstream out;
    out << "{\n  \"module\": ";
    EscapeJson(out, file.relativeStem);
    out << ",\n  \"shaders\": [";
    bool first = true;
    for (const GeneratedShader& shader : file.shaders) RenderShader(out, shader, first);
    out << "\n  ],\n  \"annotations\": [";
    first = true;
    for (const GeneratedAnnotation& annotation : file.annotations) {
        RenderAnnotation(out, annotation, first);
    }
    out << "\n  ],\n  \"assets\": [";
    for (size_t index = 0; index < file.sidecars.size(); ++index) {
        const GeneratedSidecar& asset = file.sidecars[index];
        if (index != 0) out << ',';
        out << "\n    {\"path\": "; EscapeJson(out, asset.relativePath);
        out << ", \"kind\": "; EscapeJson(out, asset.kind); out << '}';
    }
    out << "\n  ]\n}\n";
    return out.str();
}

} // namespace ConcordScript
