// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_WORKSPACELAYOUTS_H
#define CONCORD_WORKSPACELAYOUTS_H
#include <filesystem>
#include <memory>

namespace Concord::Editor {
/** Hello ImGui named layouts persisted independently for each project, workspace and width class. */
class WorkspaceLayouts {
public:
    WorkspaceLayouts();
    ~WorkspaceLayouts();
    WorkspaceLayouts(const WorkspaceLayouts&)=delete;
    WorkspaceLayouts& operator=(const WorkspaceLayouts&)=delete;
    /** Opens a project's layout store. The caller saves the previous project before switching. */
    void Open(const std::filesystem::path& directory);
    [[nodiscard]] int LastPage() const;
    /** Must run before any dockable panels are submitted in this frame. */
    void Draw(int page,bool compact,bool& showOutput,bool restoreDefault=false,bool revealOutput=false);
    /** Captures the active layout and atomically persists layouts and output visibility. */
    void Save();
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
