// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORDESIGN_H
#define CONCORD_EDITORDESIGN_H
#include <Concord/CSvgIcon.h>
namespace Concord::Editor {
/** Shared visual language for the project manager and native workspaces. */
struct Design {
    static ImVec4 Accent;
    static ImVec4 Muted;
    /** Updates semantic colors after the engine applies the selected theme. */
    static void SyncTheme();
    /** Loads the embedded SVG resources once per process. */
    static void Init();
    static void Icon(const char* name,ImVec2 position,float size,ImU32 tint=IM_COL32(185,196,220,255));
    static void Image(const char* name,float size,ImU32 tint=IM_COL32(185,196,220,255));
    /** Compact SVG button with optional label and state; tooltip uses the label. */
    static bool Action(const char* id,const char* icon,const char* label=nullptr,bool active=false,bool primary=false);
    static void Heading(const char* text);
    static void Eyebrow(const char* text);
    static void Badge(const char* text,ImVec4 color=Accent);
};
}
#endif
