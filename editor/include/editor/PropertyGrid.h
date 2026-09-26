// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORPROPERTYGRID_H
#define CONCORD_EDITORPROPERTYGRID_H
#include <Concord/CScene.h>

namespace Concord::Editor::PropertyGrid {
/**
 * Two-column inspector table: translated labels on the left, full-width editors
 * on the right. Every editor ends on a single ImGui item or group, so callers can
 * query IsItemActivated/IsItemDeactivatedAfterEdit for undo checkpoints.
 */
bool Begin(const char* id);
void End();
/** Starts a row and leaves the cursor in the value column with a full-width item. */
void Row(const char* label,const char* tooltip=nullptr);
bool Float(const char* label,float& value,float speed,float minimum,float maximum,const char* format="%.3f");
bool Slider(const char* label,float& value,float minimum,float maximum,const char* format="%.2f");
bool Integer(const char* label,int& value,int minimum,int maximum);
/** X/Y/Z fields with red, green and blue axis markers. */
bool Vector(const char* label,Vec3& value,float speed,float minimum,float maximum,const char* format="%.2f");
bool Color(const char* label,ColorRGBA& value);
bool ColorFloat(const char* label,Vec3& value);
bool Check(const char* label,bool& value);
bool Combo(const char* label,int& value,const char* const* items,int count);
bool Text(const char* label,char* buffer,size_t size);
}
#endif
