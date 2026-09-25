#include "CliOptions.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace ConcordScript {

std::vector<std::filesystem::path> DiscoverSources(const CliOptions& options)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> sources;
    for (fs::recursive_directory_iterator entry(options.projectDir), end; entry != end; ++entry) {
        const fs::path& path = entry->path();
        if (entry->is_directory()) {
            const std::string name = path.filename().string();
            if (name == ".git" || name == ".svn" || name == "node_modules" ||
                path == options.outDir || entry->is_symlink()) entry.disable_recursion_pending();
        } else if (entry->is_regular_file()) {
            std::string extension = path.extension().string();
            for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (extension == ".cx") sources.push_back(path);
        }
    }
    // All paths have the same canonical root. Lexical ordering avoids repeated
    // filesystem queries in the O(n log n) sort comparator.
    std::sort(sources.begin(), sources.end());
    if (sources.empty()) throw std::runtime_error("no .cx source files found in " + options.projectDir.string());
    return sources;
}

} // namespace ConcordScript
