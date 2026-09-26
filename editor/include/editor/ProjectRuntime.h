// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORPROJECTRUNTIME_H
#define CONCORD_EDITORPROJECTRUNTIME_H
#include "editor/ProjectDocument.h"
#include <string>
namespace Concord::Editor {
/** Entry point for projects using the editor's startup-scene and window settings. */
std::string ProjectEntryScript();
/** Recognizes the unmodified managed entry, allowing Windows line endings. */
bool IsProjectEntryScript(const std::string& source);
/** Validated source ready for writing after startup resources have been parsed. */
struct PreparedProjectRuntime {
    std::string sceneSource;
    std::string runtimeSource;
};
/** Reads and validates startup resources before any generated output is replaced. */
PreparedProjectRuntime PrepareProjectRuntime(const std::filesystem::path& root,const ProjectDocument& project);
/** Checks both generated destinations before writing either; preserves user-authored code. */
void SaveProjectRuntime(const std::filesystem::path& root,const PreparedProjectRuntime& prepared);
/** Builds portable startup code with the selected UI embedded, without runtime path assumptions. */
std::string ExportProjectRuntime(const ProjectDocument& project,const std::string& uiDocument,bool physics);
}
#endif
