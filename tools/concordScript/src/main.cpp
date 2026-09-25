/*
 * concordc — the ConcordScript CLI driver.
 *
 * Usage: concordc --project <dir> --out <dir>
 *
 * Walks every .cx file under <dir>, parses it with the flex/bison
 * lexer-parser pair, translates it to C++ via CodeGen, and writes the
 * result to <out>. Also enforces the project-wide invariant that exactly
 * one .cx file contains an @entry block, and emits the @register roll-up
 * (Concord::Script::RegisterAll) once every file has been processed.
 */

#include "Ast.h"
#include "CliReport.h"
#include "CliOptions.h"
#include "CliStyle.h"
#include "CompilerDriver.h"
#include "ProjectTemplate.h"
#include "CvmBackend.h"
#include "AnnotationPass.h"
#include "AnnotationText.h"
#include "AnnotationValues.h"
#include "CodeGen.h"
#include "ModuleName.h"
#include "MetadataCodeGen.h"
#include "Registry.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

extern int yyparse();
extern int yylex_destroy();
extern void yyrestart(std::FILE*);
extern std::FILE* yyin;

namespace {

/** Writes `text` to `path`, creating parent directories as needed. */
void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    if (fs::is_regular_file(path) && fs::file_size(path) == text.size()) {
        std::ifstream previous(path, std::ios::binary);
        const std::string contents((std::istreambuf_iterator<char>(previous)), {});
        if (!previous.bad() && contents == text) return;
    }
    std::ofstream out(path, std::ios::binary);
    out << text;
    out.close();
    if (!out) throw std::runtime_error("cannot write output file: " + path.string());
}

/** Parses a single .cx file into a ConcordScript::SourceFile. */
std::optional<ConcordScript::SourceFile> ParseFile(const fs::path& path)
{
#if defined(_WIN32)
    std::FILE* file = _wfopen(path.c_str(), L"rb");
#else
    std::FILE* file = std::fopen(path.c_str(), "rb");
#endif
    if (!file) {
        ConcordScript::ReportError("cannot open " + path.string());
        return std::nullopt;
    }

    unsigned char prefix[3]{};
    const size_t count = std::fread(prefix, 1, sizeof(prefix), file);
    if (count >= 2 && ((prefix[0] == 0xff && prefix[1] == 0xfe) ||
                       (prefix[0] == 0xfe && prefix[1] == 0xff))) {
        std::fclose(file);
        ConcordScript::ReportLocated(path.string(), 1, true, "UTF-16 source is unsupported; save as UTF-8");
        return std::nullopt;
    }
    if (!(count == 3 && prefix[0] == 0xef && prefix[1] == 0xbb && prefix[2] == 0xbf)) std::rewind(file);
    ConcordScript::ResetParserResult(path.stem().string(), path.string());

    yyin = file;
    yyrestart(file);
    const int result = yyparse();

    const bool readFailed = std::ferror(file) != 0;
    yylex_destroy();
    std::fclose(file);
    if (readFailed || result != 0 || ConcordScript::ParserHadError()) {
        ConcordScript::ReportError("failed to parse " + path.string());
        return std::nullopt;
    }
    return ConcordScript::ParserResult();
}

struct GeneratedOutput {
    fs::path relative;
    std::string header;
    std::string source;
    std::vector<ConcordScript::GeneratedSidecar> sidecars;
    std::vector<ConcordScript::GeneratedShader> shaders;
    std::vector<ConcordScript::GeneratedAnnotation> annotations;
    bool usesVulkan = false;
    bool requiresRenderApi = false;
};

bool IsVulkanAnnotation(const ConcordScript::GeneratedAnnotation& annotation)
{
    std::string name = annotation.name;
    for (char& character : name) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return name == "vulkan";
}

bool IsPassAnnotation(const ConcordScript::GeneratedAnnotation& annotation)
{
    std::string name = annotation.name;
    for (char& character : name) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (name == "pass" || name == "vulkan_pass" || name == "vulkan-pass" ||
        name == "render_pass" || name == "render-pass" || name == "custom_pass" ||
        name == "custom-pass" || name == "render.pass") {
        return true;
    }
    return name == "vulkan" &&
           ConcordScript::AnnotationPass::HasCallbackArgument(annotation.parsedArguments);
}

std::string PassName(const ConcordScript::GeneratedAnnotation& annotation)
{
    for (const ConcordScript::AnnotationArg& argument : annotation.parsedArguments) {
        if (!argument.hasKey) continue;
        std::string key = argument.key;
        for (char& character : key) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (key == "name") return ConcordScript::AnnotationValues::Unquote(argument.value);
    }
    for (const ConcordScript::AnnotationArg& argument : annotation.parsedArguments) {
        if (!argument.hasKey) return ConcordScript::AnnotationValues::Unquote(argument.value);
    }
    return {};
}

/** Rejects sidecar paths that could escape the requested output directory. */
bool IsSafeSidecarPath(const std::string& value)
{
    return ConcordScript::AnnotationText::IsSafePath(value);
}

std::string PathKey(const std::string& value)
{
    std::string key = value;
    for (char& character : key) {
        if (character == '\\') character = '/';
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return key;
}

/** Emits a JSON string with all control characters escaped. */
void EscapeJsonString(std::ostringstream& out, const std::string& value)
{
    out << '"';
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (character < 0x20) {
                out << "\\u00" << digits[(character >> 4) & 0xf] << digits[character & 0xf];
            } else {
                out << static_cast<char>(character);
            }
        }
    }
    out << '"';
}

/** Renders a CMake fragment listing inline shader sources for build tooling. */
std::string RenderShaderCmake(const std::vector<GeneratedOutput>& outputs)
{
    const auto escape = [](std::string value) {
        std::string escaped;
        for (const char character : value) {
            if (character == '\\' || character == '"' || character == ';' || character == '$') {
                escaped += '\\';
            }
            if (character == '\r' || character == '\n') continue;
            escaped += character;
        }
        return escaped;
    };
    const auto emitOptionList = [&](std::ostringstream& destination, const char* key,
                                    const std::vector<std::string>& values) {
        if (values.empty()) return;
        destination << "\n      " << key;
        for (const std::string& value : values) {
            destination << " \"" << escape(value) << "\"";
        }
    };
    const auto emitShaderCall = [&](std::ostringstream& destination,
                                    const ConcordScript::GeneratedShader& shader,
                                    bool external) {
        destination << "    concord_add_shader_sources(${target}\n"
                    << "      SOURCES \""
                    << (external ? "${CONCORDSCRIPT_EXTERNAL_SHADER_ROOT}/"
                                 : "${CONCORDSCRIPT_GENERATED_ROOT}/")
                    << escape(shader.relativePath) << "\"\n"
                    << "      STAGES \"" << escape(shader.stage) << "\"\n"
                    << "      ENTRIES \"" << escape(shader.entry) << "\"\n"
                    << "      LANGUAGES \"" << escape(shader.language) << "\"";
        emitOptionList(destination, "DEFINES", shader.defines);
        emitOptionList(destination, "INCLUDE_DIRS", shader.includeDirs);
        emitOptionList(destination, "OPTIONS", shader.options);
        destination << ")\n";
    };
    std::ostringstream out;
    out << "# Generated by concordc; do not edit.\n";
    out << "set(CONCORDSCRIPT_GENERATED_ROOT \"${CMAKE_CURRENT_LIST_DIR}\")\n";
    out << "if(NOT DEFINED CONCORDSCRIPT_EXTERNAL_SHADER_ROOT)\n"
        << "  set(CONCORDSCRIPT_EXTERNAL_SHADER_ROOT \"${CMAKE_CURRENT_LIST_DIR}\" CACHE PATH\n"
        << "      \"Root used to resolve external ConcordScript shader paths\")\n"
        << "endif()\n";
    const bool usesVulkan = std::any_of(outputs.begin(), outputs.end(),
                                        [](const GeneratedOutput& output) { return output.usesVulkan; });
    out << "set(CONCORDSCRIPT_USES_VULKAN " << (usesVulkan ? "TRUE" : "FALSE") << ")\n";
    out << "set(CONCORDSCRIPT_SHADER_SOURCES\n";
    out << "  # Entries are in the same order as STAGES and ENTRIES.\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (!shader.external) {
                out << "  \"${CONCORDSCRIPT_GENERATED_ROOT}/" << escape(shader.relativePath) << "\"\n";
            }
        }
    }
    out << ")\nset(CONCORDSCRIPT_SHADER_STAGES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (!shader.external) out << "  \"" << escape(shader.stage) << "\"\n";
        }
    }
    out << ")\nset(CONCORDSCRIPT_SHADER_ENTRIES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (!shader.external) out << "  \"" << escape(shader.entry) << "\"\n";
        }
    }
    out << ")\nset(CONCORDSCRIPT_SHADER_LANGUAGES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (!shader.external) out << "  \"" << escape(shader.language) << "\"\n";
        }
    }
    out << ")\nset(CONCORDSCRIPT_EXTERNAL_SHADER_SOURCES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (shader.external) {
                out << "  \"${CONCORDSCRIPT_EXTERNAL_SHADER_ROOT}/"
                    << escape(shader.relativePath) << "\"\n";
            }
        }
    }
    out << ")\nset(CONCORDSCRIPT_EXTERNAL_SHADER_STAGES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (shader.external) out << "  \"" << escape(shader.stage) << "\"\n";
        }
    }
    out << ")\nset(CONCORDSCRIPT_EXTERNAL_SHADER_ENTRIES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (shader.external) out << "  \"" << escape(shader.entry) << "\"\n";
        }
    }
    out << ")\nset(CONCORDSCRIPT_EXTERNAL_SHADER_LANGUAGES\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (shader.external) out << "  \"" << escape(shader.language) << "\"\n";
        }
    }
    out << ")\nset(CONCORDSCRIPT_SHADER_MANIFEST\n"
        << "  \"${CONCORDSCRIPT_GENERATED_ROOT}/ConcordScriptManifest.json\"\n)\n";
    out << "function(concordscript_attach_shaders target)\n"
        << "  if(COMMAND concord_add_shader_sources AND CONCORDSCRIPT_SHADER_SOURCES)\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (!shader.external) emitShaderCall(out, shader, false);
        }
    }
    out << "  else()\n"
        << "    message(STATUS \"ConcordScript shader helper unavailable or no inline shaders\")\n"
        << "  endif()\n"
        << "endfunction()\n";
    out << "function(concordscript_attach_external_shaders target)\n"
        << "  if(COMMAND concord_add_shader_sources AND CONCORDSCRIPT_EXTERNAL_SHADER_SOURCES)\n";
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (shader.external) emitShaderCall(out, shader, true);
        }
    }
    out << "  else()\n"
        << "    message(STATUS \"No external ConcordScript shaders requested\")\n"
        << "  endif()\n"
        << "endfunction()\n";
    out << "function(concordscript_attach_vulkan target)\n"
        << "  if(CONCORDSCRIPT_USES_VULKAN)\n"
        << "    if(NOT TARGET concord::vulkan_sdk)\n"
        << "      message(FATAL_ERROR \"ConcordScript @vulkan requires concord::vulkan_sdk\")\n"
        << "    endif()\n"
        << "    target_link_libraries(${target} PRIVATE concord::vulkan_sdk)\n"
        << "  endif()\n"
        << "endfunction()\n";
    return out.str();
}

/** Escapes one string for the project-level JSON manifest. */
void EscapeJson(std::ostringstream& out, const std::string& value)
{
    EscapeJsonString(out, value);
}

void RenderJsonStringArray(std::ostringstream& out,
                           const std::vector<std::string>& values)
{
    out << '[';
    for (size_t index = 0; index < values.size(); ++index) {
        if (index != 0) out << ',';
        EscapeJson(out, values[index]);
    }
    out << ']';
}

/** Renders a project-level shader manifest consumed by build tooling. */
std::string RenderProjectManifest(const std::vector<GeneratedOutput>& outputs)
{
    std::ostringstream out;
    out << "{\n  \"metadata\": {\"header\": \"ConcordScriptMetadata.gen.h\", "
        << "\"version\": 1},\n  \"modules\": [";
    bool firstModule = true;
    for (const GeneratedOutput& output : outputs) {
        const bool hasManifest = std::any_of(output.sidecars.begin(), output.sidecars.end(),
                                             [](const ConcordScript::GeneratedSidecar& sidecar) {
                                                 return sidecar.kind == "manifest";
                                             });
        if (!hasManifest) continue;
        if (!firstModule) out << ',';
        firstModule = false;
        const std::string module = output.relative.generic_string();
        out << "\n    {\"name\": "; EscapeJson(out, module);
        out << ", \"manifest\": "; EscapeJson(out, module + ".annotations.json");
        out << ", \"usesVulkan\": " << (output.usesVulkan ? "true" : "false") << '}';
    }
    out << "\n  ],\n  \"shaders\": [";
    bool first = true;
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            if (!first) out << ',';
            first = false;
            out << "\n    {\"name\": "; EscapeJson(out, shader.name);
            out << ", \"stage\": "; EscapeJson(out, shader.stage);
            out << ", \"entry\": "; EscapeJson(out, shader.entry);
            out << ", \"language\": "; EscapeJson(out, shader.language);
            out << ", \"defines\": ";
            RenderJsonStringArray(out, shader.defines);
            out << ", \"includeDirs\": ";
            RenderJsonStringArray(out, shader.includeDirs);
            out << ", \"options\": ";
            RenderJsonStringArray(out, shader.options);
            out << ", \"path\": "; EscapeJson(out, shader.relativePath);
            out << ", \"external\": " << (shader.external ? "true" : "false") << '}';
        }
    }
    out << "\n  ],\n  \"annotations\": [";
    bool firstAnnotation = true;
    for (const GeneratedOutput& output : outputs) {
        for (const ConcordScript::GeneratedAnnotation& annotation : output.annotations) {
            if (!firstAnnotation) out << ',';
            firstAnnotation = false;
            out << "\n    {\"module\": ";
            EscapeJson(out, output.relative.generic_string());
            out << ", \"name\": "; EscapeJson(out, annotation.name);
            out << ", \"args\": "; EscapeJson(out, annotation.arguments);
            out << ", \"body\": "; EscapeJson(out, annotation.body);
            out << ", \"owner\": "; EscapeJson(out, annotation.owner);
            out << ", \"line\": " << annotation.line << ", \"parameters\": [";
            for (size_t index = 0; index < annotation.parsedArguments.size(); ++index) {
                const ConcordScript::AnnotationArg& parameter = annotation.parsedArguments[index];
                if (index != 0) out << ',';
                out << "{\"key\": "; EscapeJson(out, parameter.key);
                out << ", \"value\": "; EscapeJson(out, parameter.value);
                out << ", \"raw\": "; EscapeJson(out, parameter.raw);
                out << ", \"named\": " << (parameter.hasKey ? "true" : "false") << '}';
            }
            out << "]}";
        }
    }
    out << "\n  ]\n}\n";
    return out.str();
}

/** Rejects duplicate stems before parsing can produce colliding outputs. */
bool ValidateSourceFiles(const std::vector<fs::path>& sourceFiles,
                         const fs::path& projectDir)
{
    std::unordered_map<std::string, fs::path> modules;
    for (const fs::path& path : sourceFiles) {
        const std::string module = path.stem().string();
        if (!ConcordScript::ModuleName::IsValid(module)) {
            ConcordScript::ReportError("invalid module name '" + module + "' in " + path.string());
            return false;
        }
        const auto [it, inserted] = modules.emplace(PathKey(module), path);
        if (!inserted) {
            ConcordScript::ReportError(
                "namespace collision (case-insensitive) '" + module + "' between " +
                fs::relative(it->second, projectDir).generic_string() + " and " +
                fs::relative(path, projectDir).generic_string());
            return false;
        }
    }
    return true;
}

} // namespace

namespace ConcordScript {

static int RunCompilerImpl(int argc, char** argv, bool defaultToCvm)
{
    const CliOptions options = ParseArgs(argc, argv, defaultToCvm);
    if (options.help) {
        PrintHelp(defaultToCvm);
        return 0;
    }
    if (options.version) {
        PrintVersion();
        return 0;
    }
    if (!options.initDir.empty()) {
        InitializeProject(options.initDir);
        return 0;
    }

    const std::vector<fs::path> sourceFiles = DiscoverSources(options);
    if (!ValidateSourceFiles(sourceFiles, options.projectDir)) {
        return 1;
    }

    std::vector<ConcordScript::SourceFile> parsedFiles;
    std::vector<fs::path> parsedPaths;
    for (const fs::path& path : sourceFiles) {
        std::optional<ConcordScript::SourceFile> parsed = ParseFile(path);
        if (!parsed) {
            return 1;
        }
        parsedPaths.push_back(path);
        parsedFiles.push_back(std::move(*parsed));
    }

    if (!options.cpp) {
        return CompileCvmProject(parsedFiles, options.outDir,
                                 ConcordScript::CvmOptions{options.cvmMode, options.runCvm,
                                                           options.cvmHosts});
    }

    std::vector<std::string> registeredClasses;
    std::vector<std::string> registeredHeaderPaths;
    std::vector<std::string> systemClasses;
    std::vector<std::string> systemHeaderPaths;
    std::vector<GeneratedOutput> generatedOutputs;
    std::vector<ConcordScript::MetadataModule> metadataModules;
    fs::path entryFile;
    int fileCount = 0;

    for (size_t index = 0; index < parsedFiles.size(); ++index) {
        const fs::path& path = parsedPaths[index];
        ConcordScript::SourceFile& parsed = parsedFiles[index];

        const size_t classCountBefore = registeredClasses.size();
        const size_t systemCountBefore = systemClasses.size();
        ConcordScript::GeneratedFile generated =
            ConcordScript::TranslateFile(parsed, registeredClasses, systemClasses);

        bool hasAnnotationError = false;
        for (const ConcordScript::GeneratedDiagnostic& diagnostic : generated.diagnostics) {
            ReportLocated(path.string(), diagnostic.line, diagnostic.isError, diagnostic.message);
            hasAnnotationError = hasAnnotationError || diagnostic.isError;
        }
        if (hasAnnotationError) {
            return 1;
        }

        if (generated.hasEntry) {
            if (!entryFile.empty()) {
                ReportError("multiple @entry blocks found (" + entryFile.string() + " and " +
                            path.string() + "); a project may have exactly one");
                return 1;
            }
            entryFile = path;
        }

        const fs::path relative = fs::relative(path, options.projectDir).replace_extension();
        generated.relativeStem = relative.generic_string();
        for (ConcordScript::GeneratedSidecar& sidecar : generated.sidecars) {
            if (sidecar.kind == "manifest") {
                sidecar.relativePath = relative.generic_string() + ".annotations.json";
                sidecar.text = ConcordScript::RenderAnnotationManifest(generated);
            }
        }
        const std::string headerPath = relative.generic_string() + ".gen.h";
        metadataModules.push_back(ConcordScript::MetadataModule{
            .module = relative.generic_string(),
            .annotations = generated.annotations,
            .shaders = generated.shaders,
        });
        const bool usesVulkan = generated.usesVulkan ||
                                std::any_of(generated.annotations.begin(), generated.annotations.end(),
                                            IsVulkanAnnotation);
        generatedOutputs.push_back(GeneratedOutput{
            .relative = relative,
            .header = std::move(generated.headerText),
            .source = std::move(generated.sourceText),
            .sidecars = std::move(generated.sidecars),
            .shaders = std::move(generated.shaders),
            .annotations = std::move(generated.annotations),
            .usesVulkan = usesVulkan,
            .requiresRenderApi = generated.requiresRenderApi,
        });
        ++fileCount;

        // Every class this file registered shares this same generated header.
        for (size_t i = classCountBefore; i < registeredClasses.size(); ++i) {
            registeredHeaderPaths.push_back(headerPath);
        }
        for (size_t i = systemCountBefore; i < systemClasses.size(); ++i) {
            systemHeaderPaths.push_back(headerPath);
        }
    }

    if (entryFile.empty()) {
        ReportError("no @entry block found; a project needs exactly one");
        return 1;
    }

    std::unordered_map<std::string, fs::path> sidecarPaths;
    std::unordered_map<std::string, fs::path> shaderPaths;
    std::unordered_map<std::string, fs::path> shaderNames;
    std::unordered_map<std::string, fs::path> passNames;
    std::unordered_map<std::string, fs::path> reservedPaths = {
        {PathKey("ConcordScriptRegistry.gen.h"), fs::path("<registry>")},
        {PathKey("ConcordScriptRegistry.gen.cpp"), fs::path("<registry>")},
        {PathKey("ConcordScriptManifest.json"), fs::path("<manifest>")},
        {PathKey("ConcordScriptShaders.cmake"), fs::path("<cmake>")},
        {PathKey("ConcordScriptMetadata.gen.h"), fs::path("<metadata>")},
    };
    for (const GeneratedOutput& output : generatedOutputs) {
        const std::string stem = output.relative.generic_string();
        reservedPaths.emplace(PathKey(stem + ".gen.h"), output.relative);
        reservedPaths.emplace(PathKey(stem + ".gen.cpp"), output.relative);
    }
    for (const GeneratedOutput& output : generatedOutputs) {
        for (const ConcordScript::GeneratedAnnotation& annotation : output.annotations) {
            if (!IsPassAnnotation(annotation)) continue;
            const std::string name = PassName(annotation);
            if (name.empty()) continue;
            const std::string key = PathKey(name);
            const auto [passIt, passInserted] = passNames.emplace(key, output.relative);
            if (!passInserted) {
                ReportError("duplicate Vulkan pass '" + name + "' (" + passIt->second.string() +
                            " and " + output.relative.string() + ")");
                return 1;
            }
        }
        for (const ConcordScript::GeneratedShader& shader : output.shaders) {
            const auto reserved = reservedPaths.find(PathKey(shader.relativePath));
            if (reserved != reservedPaths.end()) {
                ReportError("shader path is reserved '" + shader.relativePath + "'");
                return 1;
            }
            const std::string key = PathKey(shader.name + "@" + shader.stage);
            const auto [nameIt, nameInserted] = shaderNames.emplace(key, output.relative);
            if (!nameInserted) {
                ReportError("duplicate shader '" + shader.name + "' stage '" + shader.stage + "' (" +
                            nameIt->second.string() + " and " + output.relative.string() + ")");
                return 1;
            }
            const std::string shaderPathKey = PathKey(shader.relativePath);
            const auto [pathIt, pathInserted] = shaderPaths.emplace(shaderPathKey, output.relative);
            if (!pathInserted) {
                ReportError("shader path collision '" + shader.relativePath + "' (" +
                            pathIt->second.string() + " and " + output.relative.string() + ")");
                return 1;
            }
        }
        for (const ConcordScript::GeneratedSidecar& sidecar : output.sidecars) {
            if (!IsSafeSidecarPath(sidecar.relativePath)) {
                ReportError("unsafe generated path '" + sidecar.relativePath + "'");
                return 1;
            }
            const std::string sidecarKey = PathKey(sidecar.relativePath);
            if (reservedPaths.find(sidecarKey) != reservedPaths.end()) {
                ReportError("generated path is reserved '" + sidecar.relativePath + "'");
                return 1;
            }
            const auto shaderIt = shaderPaths.find(sidecarKey);
            if (shaderIt != shaderPaths.end() && sidecar.kind != "shader") {
                ReportError("generated path collides with shader '" + sidecar.relativePath + "'");
                return 1;
            }
            const auto [it, inserted] = sidecarPaths.emplace(sidecarKey, output.relative);
            if (!inserted) {
                ReportError("generated path collision '" + sidecar.relativePath + "' (" +
                            it->second.string() + " and " + output.relative.string() + ")");
                return 1;
            }
        }
    }

    if (options.check) {
        ReportSuccess("checked " + std::to_string(fileCount) + " file(s); no output written");
        return 0;
    }

    for (GeneratedOutput& output : generatedOutputs) {
        WriteFile(options.outDir / (output.relative.string() + ".gen.h"), output.header);
        WriteFile(options.outDir / (output.relative.string() + ".gen.cpp"), output.source);
        for (const ConcordScript::GeneratedSidecar& sidecar : output.sidecars) {
            WriteFile(options.outDir / fs::path(sidecar.relativePath), sidecar.text);
        }
    }
    WriteFile(options.outDir / "ConcordScriptRegistry.gen.h", ConcordScript::GenerateRegistryHeader());
    WriteFile(options.outDir / "ConcordScriptRegistry.gen.cpp",
             ConcordScript::GenerateRegistrySource(registeredClasses, registeredHeaderPaths,
                                                   systemClasses, systemHeaderPaths));
    WriteFile(options.outDir / "ConcordScriptManifest.json", RenderProjectManifest(generatedOutputs));
    WriteFile(options.outDir / "ConcordScriptMetadata.gen.h",
             ConcordScript::RenderMetadataHeader(metadataModules));
    std::string shaderCmake = RenderShaderCmake(generatedOutputs);
    shaderCmake += ConcordScript::RenderMetadataCmake(metadataModules);
    WriteFile(options.outDir / "ConcordScriptShaders.cmake", shaderCmake);

    ReportSuccess("translated " + std::to_string(fileCount) + " file(s), " +
                  std::to_string(registeredClasses.size()) + " registered class(es)");
    return 0;
}

int RunCompiler(int argc, char** argv, bool defaultToCvm)
{
    try {
        return RunCompilerImpl(argc, argv, defaultToCvm);
    } catch (const std::exception& error) {
        ReportError(error.what());
        return 1;
    }
}

} // namespace ConcordScript
