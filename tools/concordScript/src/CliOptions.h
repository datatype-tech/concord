#ifndef CONCORDSCRIPT_CLIOPTIONS_H
#define CONCORDSCRIPT_CLIOPTIONS_H

#include "CliStyle.h"
#include "CvmBackend.h"

#include <filesystem>
#include <vector>

namespace ConcordScript {

/** Validated command-line settings shared by both compiler personalities. */
struct CliOptions {
    std::filesystem::path initDir;
    std::filesystem::path projectDir;
    std::filesystem::path outDir;
    bool cpp = false;
    CvmMode cvmMode = CvmMode::Jit;
    bool runCvm = true;
    std::vector<std::filesystem::path> cvmHosts;
    ColorMode color = ColorMode::Auto;
    bool help = false;
    bool version = false;
    bool check = false;
};

/** Throws a diagnostic on invalid or conflicting arguments. */
CliOptions ParseArgs(int argc, char** argv, bool defaultToCvm);

/** Discovers sources deterministically, excluding output and tool directories. */
std::vector<std::filesystem::path> DiscoverSources(const CliOptions& options);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CLIOPTIONS_H
