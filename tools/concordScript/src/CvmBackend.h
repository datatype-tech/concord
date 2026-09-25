#ifndef CONCORDSCRIPT_CVMBACKEND_H
#define CONCORDSCRIPT_CVMBACKEND_H

#include "Ast.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ConcordScript {

/** Selects the execution or object-emission path for the direct LLVM backend. */
enum class CvmMode { Jit, Aot };

/** Everything the CVM backend needs beyond the parsed sources. */
struct CvmOptions {
    /** Which of the two artifact paths to take. */
    CvmMode mode = CvmMode::Jit;

    /** Whether the JIT path runs the entry point after compiling it. */
    bool run = true;

    /**
     * Modules whose exports satisfy the registered host-module surface, from
     * --cvm-host, in the order they were named. Empty means the Concord
     * binding is unavailable, which is only a problem for a program that
     * actually calls it.
     *
     * More than one is the normal case for the engine: Runtime.dll carries the
     * C ABI, but a render backend only registers once Render.dll is loaded, so
     * a script that opens a window needs both.
     */
    std::vector<std::filesystem::path> hostModules;
};

/** Compiles a project-level `@cvm` block directly to LLVM IR. */
int CompileCvmProject(const std::vector<SourceFile>& files,
                      const std::filesystem::path& outputDirectory,
                      const CvmOptions& options);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CVMBACKEND_H
