// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Design.h"
#include "EditorIcons.gen.h"
#include <map>
#include <string>
#include <algorithm>
#include <cmath>
namespace Concord::Editor {
ImVec4 Design::Accent{};
ImVec4 Design::Muted{};
namespace {std::map<std::string,std::unique_ptr<SvgIcon>> icons;}
void Design::SyncTheme()
{
    Accent=ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    Muted=ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}
void Design::Init()
{
    if(!icons.empty())return;
    for(const auto& resource:IconResources) {
        auto icon=std::make_unique<SvgIcon>();
        if(icon->Load(resource.source))icons.emplace(resource.name,std::move(icon));
    }
}
void Design::Icon(const char* name,ImVec2 position,float size,ImU32 tint)
{
    auto found=icons.find(name);if(found!=icons.end())found->second->Draw(*ImGui::GetWindowDrawList(),position,size,tint);
}
void Design::Image(const char* name,float size,ImU32 tint)
{
    Icon(name,ImGui::GetCursorScreenPos(),size,tint);ImGui::Dummy({size,size});
}
bool Design::Action(const char* id,const char* icon,const char* label,bool active,bool primary)
{
    const float unit=ImGui::GetFontSize(),height=ImGui::GetFrameHeight();
    const float padding=ImGui::GetStyle().FramePadding.x;
    const float width=label?unit+ImGui::CalcTextSize(label).x+padding*3:height;
    const auto position=ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(primary?ImGuiCol_ButtonActive:active?ImGuiCol_Header:ImGuiCol_Button));
    const bool clicked=ImGui::Button((std::string("##")+id).c_str(),{width,height});ImGui::PopStyleColor();
    const float hovered=ImGui::IsItemHovered()?1.0f:0.0f;
    const ImGuiID animationId=ImGui::GetItemID();
    ImGuiStorage* animation=ImGui::GetStateStorage();
    const float before=animation->GetFloat(animationId,0.0f);
    const float glow=before+(hovered-before)*std::min(1.0f,ImGui::GetIO().DeltaTime*14.0f);
    animation->SetFloat(animationId,glow);
    if(glow>0.01f) {
        const ImVec4 accent=ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        ImGui::GetWindowDrawList()->AddRect(position,{position.x+width,position.y+height},
            ImGui::GetColorU32({accent.x,accent.y,accent.z,glow*0.6f}),
            ImGui::GetStyle().FrameRounding,0,1.5f);
    }
    const auto color=ImGui::GetColorU32(ImGuiCol_Text);
    Icon(icon,{position.x+(label?padding:(width-unit)/2),position.y+(height-unit)/2},unit,color);
    if(label)ImGui::GetWindowDrawList()->AddText({position.x+unit+padding*2,position.y+(height-unit)/2},color,label);
    else if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",id);
    return clicked;
}
void Design::Heading(const char* text)
{
    auto& fonts=ImGui::GetIO().Fonts->Fonts;
    if(fonts.Size>2)ImGui::PushFont(fonts[2]);ImGui::TextUnformatted(text);if(fonts.Size>2)ImGui::PopFont();
}
void Design::Eyebrow(const char* text) {ImGui::TextColored(Muted,"%s",text);}
void Design::Badge(const char* text,ImVec4 color)
{
    auto position=ImGui::GetCursorScreenPos();auto size=ImGui::CalcTextSize(text);size.x+=14;size.y+=6;
    ImGui::GetWindowDrawList()->AddRectFilled(position,{position.x+size.x,position.y+size.y},ImGui::GetColorU32({color.x*0.20f,color.y*0.20f,color.z*0.20f,1}),4);
    ImGui::GetWindowDrawList()->AddText({position.x+7,position.y+3},ImGui::GetColorU32(color),text);ImGui::Dummy(size);
}
}
