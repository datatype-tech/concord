// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_SVGICON_H
#define CONCORD_SVGICON_H
#include "Concord/CExport.h"
#include <imgui.h>
#include <memory>
#include <string>
namespace Concord {
/** Scalable monochrome UI artwork parsed from SVG paths.
 * Supports solid fills and strokes; gradients, masks and compound holes are excluded. */
class CENGINE_API SvgIcon {
public:
    SvgIcon();
    ~SvgIcon();
    SvgIcon(const SvgIcon&)=delete;
    SvgIcon& operator=(const SvgIcon&)=delete;
    /** Parses SVG source without external URLs, fonts or scripts. */
    bool Load(const std::string& source);
    /** Emits vector curves directly into the native UI draw list. */
    void Draw(ImDrawList& draw,ImVec2 position,float size,ImU32 tint) const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
#endif
