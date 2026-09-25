#ifndef CONCORDSCRIPT_CODEGEN_H
#define CONCORDSCRIPT_CODEGEN_H

#include "Ast.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ConcordScript {

/** A generated text file requested by a ConcordScript annotation. */
struct GeneratedSidecar {
    /** Path relative to concordc's output directory, using `/` separators. */
    std::string relativePath;

    /** UTF-8 text written to relativePath. */
    std::string text;

    /** Stable asset category (`shader`, `manifest`, or `text`). */
    std::string kind = "text";
};

/** Metadata for a shader emitted from an `@shader` annotation. */
struct GeneratedShader {
    /** Logical shader identifier supplied by `name=` or generated. */
    std::string name;
    /** Normalized Vulkan stage suffix (vert/frag/comp/rgen/...). */
    std::string stage;
    /** Entry point, defaulting to `main`. */
    std::string entry;
    /** Source language, defaulting to `glsl`. */
    std::string language;
    /** Preprocessor definitions passed to the shader compiler. */
    std::vector<std::string> defines;
    /** Include search directories passed to the shader compiler. */
    std::vector<std::string> includeDirs;
    /** Additional compiler flags passed verbatim as individual arguments. */
    std::vector<std::string> options;
    /** Safe path relative to the output directory. */
    std::string relativePath;
    /** True when the path refers to a user-managed external source. */
    bool external = false;
    /** AST owner (`Module::Class`, `Module::entry`, etc.). */
    std::string owner;
    /** One-based source line. */
    int line = 0;
};

/** A normalized annotation record retained in the generated manifest. */
struct GeneratedAnnotation {
    std::string name;
    std::string arguments;
    std::string body;
    std::string owner;
    int line = 0;
    std::vector<AnnotationArg> parsedArguments;
};

/** A source-located warning or error produced while lowering an annotation. */
struct GeneratedDiagnostic {
    std::string message;
    int line = 0;
    bool isError = true;
};

/** One .cx file's translation output. */
struct GeneratedFile {
    using Sidecar = GeneratedSidecar;

    std::string headerText;
    std::string sourceText;
    std::string tailSourceText;

    /** Relative path (no extension) used for both .gen.h and .gen.cpp. */
    std::string relativeStem;

    /** Whether this file contained an `@entry` block. */
    bool hasEntry = false;
    bool requiresRenderApi = false;
    bool usesVulkan = false;

    /** Additional files requested by annotations in source order. */
    std::vector<GeneratedSidecar> sidecars;

    /** Shader metadata in the same order as their sidecar files. */
    std::vector<GeneratedShader> shaders;

    /** All annotations retained for build tools and editor integrations. */
    std::vector<GeneratedAnnotation> annotations;

    /** Validation messages that must be reported before any files are written. */
    std::vector<GeneratedDiagnostic> diagnostics;
};

/**
 * Translates one parsed .cx file to C++.
 *
 * Wraps the file's contents in `namespace <moduleName> { ... }`, rewrites
 * `use` directives to #include, substitutes var/pub/priv/prot inside
 * captured bodies, and renders extends/implements as a base-class list.
 * Class definitions land in the header (so other files' `use` can see
 * them); @entry and free-standing raw code land in the source file.
 *
 * @param registeredClasses Receives the fully-qualified name of every
 *        `@register`-tagged class found in this file, appended in order.
 */
GeneratedFile TranslateFile(const SourceFile& file, std::vector<std::string>& registeredClasses,
                            std::vector<std::string>& systemClasses);

/** Renders a deterministic JSON manifest for annotation-driven build tooling. */
std::string RenderAnnotationManifest(const GeneratedFile& file);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CODEGEN_H
