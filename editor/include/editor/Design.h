// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORDESIGN_H
#define CONCORD_EDITORDESIGN_H
#include <Concord/CSvgIcon.h>
#include <Concord/CUiToolkit.h>
namespace Concord::Editor {
/** One option of a segmented control. */
struct Segment {
    const char* icon;
    const char* label;
};
/** Glyphs drawn by Windows 11 style caption buttons. */
enum class CaptionGlyph { Minimize, Maximize, Restore, Close, Fullscreen, ExitFullscreen };
/** Shared visual language for the project manager and native workspaces. */
struct Design {
    static ImVec4 Accent;
    static ImVec4 Muted;
    /** Updates semantic colors after the engine applies the selected theme. */
    static void SyncTheme();
    /** Loads action icons and queues the embedded Concord Flash logo as GPU textures. */
    static void Init(UiToolkit* toolkit);
    static void Icon(const char* name,ImVec2 position,float size,ImU32 tint=IM_COL32(185,196,220,255));
    static void Image(const char* name,float size,ImU32 tint=IM_COL32(185,196,220,255));
    /** Draws the official logo; a plain disc stands in for the first frame until its texture exists. */
    static void Logo(ImVec2 position,float size);
    /** Logo as a layout item. */
    static void LogoImage(float size);
    /** Rounded button with optional label; primary uses the accent fill. Icon-only buttons show a translated tooltip. */
    static bool Action(const char* id,const char* icon,const char* label=nullptr,bool active=false,bool primary=false);
    /** Frameless toolbar button whose background only appears on hover or when active. */
    static bool Ghost(const char* id,const char* icon,const char* label=nullptr,bool active=false,float height=0);
    /** Segmented control; returns the clicked index, or -1. */
    static int Segmented(const char* id,const Segment* segments,int count,int selected,float height=0);
    /** Windows 11 caption button filling the given size; close turns red on hover. */
    static bool CaptionButton(const char* id,CaptionGlyph glyph,ImVec2 size,bool dimmed);
    /** Eases a per-item value toward on/off, for hover fades. */
    static float Fade(ImGuiID id,bool on,float speed=16.0f);
    static void Spinner(float radius,ImU32 color);
    static void Heading(const char* text);
    static void Eyebrow(const char* text);
    static void Badge(const char* text,ImVec4 color=Accent);
    /** Tooltip for the previous item after the platform hover delay. */
    static void Tooltip(const char* text);
    /**
     * Modal with a fixed width that stays centered every frame, so changing its
     * content or language never moves it sideways. Pair a true result with EndPopup.
     */
    static bool BeginDialog(const char* titleId,float widthInFontUnits,bool* open=nullptr);
    /** Right-aligns the next row of buttons whose total width is known. */
    static void AlignRight(float width);
    /** Width of an Action/Ghost button with the given label. */
    static float ButtonWidth(const char* label,bool icon=true);
    static ImU32 Blend(ImU32 from,ImU32 to,float amount);
};
}
#endif
