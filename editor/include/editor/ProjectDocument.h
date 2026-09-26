// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_PROJECTDOCUMENT_H
#define CONCORD_PROJECTDOCUMENT_H
#include <filesystem>
#include <string>

namespace Concord::Editor {
/** Versioned Concord.project configuration; paths are UTF-8 and relative to the project root. */
class ProjectDocument {
public:
    std::string name="My Concord Game";
    std::string startupScene="Scenes/Main.scene";
    std::string startupUi;
    int width=1280,height=800;
    bool decorated=true,resizable=true,fullscreen=false,vsync=true;
    unsigned int targetFps=120;
    /** Serializes only validated settings. Zero targetFps means unbounded. */
    std::string Serialize() const;
    /** Accepts omitted startupUi and targetFps; failure leaves this document unchanged. */
    void Parse(const std::string& text);
    /** Validates values and lexical project-relative paths without requiring files to exist. */
    void Validate() const;
    /** Also resolves all referenced files inside the project, including symlink/junction targets. */
    void ValidatePaths(const std::filesystem::path& project,bool requireExisting=false) const;
};
/** Resolves a safe project-relative path; refuses traversal, drive/UNC paths, symlink escapes and Windows device names. */
std::filesystem::path ResolveProjectPath(const std::filesystem::path& project,const std::string& relative,bool requireExisting=false);
}
#endif
