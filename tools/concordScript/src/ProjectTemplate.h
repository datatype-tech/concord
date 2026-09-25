#ifndef CONCORDSCRIPT_PROJECTTEMPLATE_H
#define CONCORDSCRIPT_PROJECTTEMPLATE_H

#include <filesystem>

namespace ConcordScript {
/** Creates a C++-backend starter in a new or empty directory, without overwriting files. */
void InitializeProject(const std::filesystem::path& directory);
}

#endif // CONCORDSCRIPT_PROJECTTEMPLATE_H
