// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/PropertyGrid.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include <algorithm>
#include <cmath>

namespace Concord::Editor::PropertyGrid {
bool Begin(const char* id)
{
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{ImGui::GetStyle().CellPadding.x,ImGui::GetStyle().ItemSpacing.y*0.5f});
    if(!ImGui::BeginTable(id,2,ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_NoSavedSettings)) {ImGui::PopStyleVar();return false;}
    ImGui::TableSetupColumn("label",ImGuiTableColumnFlags_WidthFixed,std::clamp(width*0.38f,ImGui::GetFontSize()*4.5f,ImGui::GetFontSize()*9.5f));
    ImGui::TableSetupColumn("value",ImGuiTableColumnFlags_WidthStretch);
    return true;
}
void End()
{
    ImGui::EndTable();ImGui::PopStyleVar();
}
void Row(const char* label,const char* tooltip)
{
    ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text,ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    const char* text=Tr(label);
    const float available=ImGui::GetContentRegionAvail().x;
    if(ImGui::CalcTextSize(text).x>available) {
        ImGui::PushClipRect(ImGui::GetCursorScreenPos(),{ImGui::GetCursorScreenPos().x+available,ImGui::GetCursorScreenPos().y+ImGui::GetFrameHeight()},true);
        ImGui::TextUnformatted(text);ImGui::PopClipRect();
        if(!tooltip)tooltip=text;
    } else ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    if(tooltip)Design::Tooltip(Tr(tooltip));
    ImGui::TableSetColumnIndex(1);ImGui::SetNextItemWidth(-FLT_MIN);
}
bool Float(const char* label,float& value,float speed,float minimum,float maximum,const char* format)
{
    Row(label);ImGui::PushID(label);
    const bool changed=ImGui::DragFloat("##value",&value,speed,minimum,maximum,format,ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();return changed;
}
bool Slider(const char* label,float& value,float minimum,float maximum,const char* format)
{
    Row(label);ImGui::PushID(label);
    const bool changed=ImGui::SliderFloat("##value",&value,minimum,maximum,format,ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();return changed;
}
bool Integer(const char* label,int& value,int minimum,int maximum)
{
    Row(label);ImGui::PushID(label);
    const bool changed=ImGui::SliderInt("##value",&value,minimum,maximum,"%d",ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();return changed;
}
bool Vector(const char* label,Vec3& value,float speed,float minimum,float maximum,const char* format)
{
    Row(label);ImGui::PushID(label);
    const float spacing=ImGui::GetStyle().ItemInnerSpacing.x;
    const float width=(ImGui::GetContentRegionAvail().x-spacing*2)/3.0f;
    const ImU32 axes[]={IM_COL32(226,88,88,255),IM_COL32(118,196,98,255),IM_COL32(92,146,232,255)};
    float* components[]={&value.x,&value.y,&value.z};
    bool changed=false;
    ImGui::BeginGroup();
    for(int axis=0;axis<3;++axis) {
        if(axis)ImGui::SameLine(0,spacing);
        ImGui::PushID(axis);ImGui::SetNextItemWidth(width);
        const auto position=ImGui::GetCursorScreenPos();
        changed|=ImGui::DragFloat("##axis",components[axis],speed,minimum,maximum,format,ImGuiSliderFlags_AlwaysClamp);
        const float marker=std::max(2.0f,std::round(ImGui::GetFontSize()*0.14f));
        ImGui::GetWindowDrawList()->AddRectFilled(position,{position.x+marker,position.y+ImGui::GetFrameHeight()},axes[axis],
            ImGui::GetStyle().FrameRounding,ImDrawFlags_RoundCornersLeft);
        ImGui::PopID();
    }
    ImGui::EndGroup();ImGui::PopID();
    return changed;
}
bool Color(const char* label,ColorRGBA& value)
{
    Row(label);ImGui::PushID(label);
    float channels[]={ColorR(value)/255.0f,ColorG(value)/255.0f,ColorB(value)/255.0f};
    const bool changed=ImGui::ColorEdit3("##value",channels,ImGuiColorEditFlags_DisplayHex|ImGuiColorEditFlags_NoOptions);
    if(changed) {
        auto channel=[](float component){return static_cast<u8>(std::lround(std::clamp(component,0.0f,1.0f)*255));};
        value=MakeColor(channel(channels[0]),channel(channels[1]),channel(channels[2]),ColorA(value));
    }
    ImGui::PopID();return changed;
}
bool ColorFloat(const char* label,Vec3& value)
{
    Row(label);ImGui::PushID(label);
    const bool changed=ImGui::ColorEdit3("##value",&value.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_NoOptions);
    ImGui::PopID();return changed;
}
bool Check(const char* label,bool& value)
{
    Row(label);ImGui::PushID(label);
    const bool changed=ImGui::Checkbox("##value",&value);
    ImGui::PopID();return changed;
}
bool Combo(const char* label,int& value,const char* const* items,int count)
{
    Row(label);ImGui::PushID(label);
    bool changed=false;
    if(ImGui::BeginCombo("##value",value>=0 && value<count?Tr(items[value]):"")) {
        for(int index=0;index<count;++index)if(ImGui::Selectable(Tr(items[index]),index==value)){value=index;changed=true;}
        ImGui::EndCombo();
    }
    ImGui::PopID();return changed;
}
bool Text(const char* label,char* buffer,size_t size)
{
    Row(label);ImGui::PushID(label);
    const bool changed=ImGui::InputText("##value",buffer,size);
    ImGui::PopID();return changed;
}
}
