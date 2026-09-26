// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Design.h"
#include "editor/Localization.h"
#include "EditorIcons.gen.h"
#include "EditorLogo.gen.h"
#include <imgui_internal.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace Concord::Editor {
ImVec4 Design::Accent{0.36f,0.56f,0.90f,1.0f};
ImVec4 Design::Muted{0.50f,0.53f,0.58f,1.0f};
namespace {
std::map<std::string,std::unique_ptr<SvgIcon>> icons;
/** Pre-filtered logo sizes; sampling without mipmaps stays crisp only near the stored size. */
constexpr std::array<int,4> LogoSizes{40,72,144,288};
UiToolkit* logoToolkit=nullptr;
std::array<u32,LogoSizes.size()> logoImages{};

ImU32 WithAlpha(ImU32 color,float alpha)
{
    const auto a=static_cast<ImU32>(std::clamp(alpha,0.0f,1.0f)*((color>>IM_COL32_A_SHIFT)&0xFF));
    return (color&~IM_COL32_A_MASK)|(a<<IM_COL32_A_SHIFT);
}
void QueueLogo(UiToolkit& toolkit)
{
    int width=0,height=0,channels=0;
    auto* source=stbi_load_from_memory(ConcordLogoPng,static_cast<int>(sizeof(ConcordLogoPng)),&width,&height,&channels,4);
    if(!source)return;
    for(size_t index=0;index<LogoSizes.size();++index) {
        const int size=LogoSizes[index];
        std::vector<unsigned char> pixels(static_cast<size_t>(size)*size*4);
        if(stbir_resize_uint8_srgb(source,width,height,0,pixels.data(),size,size,0,STBIR_RGBA))
            logoImages[index]=toolkit.AddImage(pixels.data(),static_cast<u32>(size),static_cast<u32>(size));
    }
    stbi_image_free(source);
}
void DrawCaptionGlyph(ImDrawList& draw,CaptionGlyph glyph,ImVec2 center,float unit,ImU32 color)
{
    const float half=std::round(unit*0.27f),line=std::max(1.0f,std::round(unit/18.0f));
    const ImVec2 min{std::round(center.x-half)+0.5f,std::round(center.y-half)+0.5f},max{min.x+half*2,min.y+half*2};
    switch(glyph) {
    case CaptionGlyph::Minimize:
        draw.AddLine({min.x,std::round(center.y)+0.5f},{max.x,std::round(center.y)+0.5f},color,line);break;
    case CaptionGlyph::Maximize:
        draw.AddRect(min,max,color,line*1.5f,0,line);break;
    case CaptionGlyph::Restore: {
        const float offset=std::round(half*0.4f);
        draw.AddRect({min.x,min.y+offset},{max.x-offset,max.y},color,line*1.5f,0,line);
        draw.AddLine({min.x+offset,min.y},{max.x-line,min.y},color,line);
        draw.AddLine({max.x,min.y+line},{max.x,max.y-offset},color,line);
        break;
    }
    case CaptionGlyph::Close:
        draw.AddLine(min,max,color,line);draw.AddLine({min.x,max.y},{max.x,min.y},color,line);break;
    case CaptionGlyph::Fullscreen:
    case CaptionGlyph::ExitFullscreen: {
        const float arm=std::round(half*0.75f);const bool inward=glyph==CaptionGlyph::ExitFullscreen;
        const ImVec2 corners[]={min,{max.x,min.y},max,{min.x,max.y}};const float sx[]={1,-1,-1,1},sy[]={1,1,-1,-1};
        for(int index=0;index<4;++index) {
            auto corner=corners[index];
            if(inward){corner.x+=sx[index]*arm*0.9f;corner.y+=sy[index]*arm*0.9f;}
            const float dx=inward?-sx[index]:sx[index],dy=inward?-sy[index]:sy[index];
            draw.AddLine(corner,{corner.x+dx*arm,corner.y},color,line);
            draw.AddLine(corner,{corner.x,corner.y+dy*arm},color,line);
        }
        break;
    }
    }
}
}

void Design::SyncTheme()
{
    Accent=ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    Muted=ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}
void Design::Init(UiToolkit* toolkit)
{
    if(icons.empty()) {
        for(const auto& resource:IconResources) {
            auto icon=std::make_unique<SvgIcon>();
            if(icon->Load(resource.source))icons.emplace(resource.name,std::move(icon));
        }
    }
    if(toolkit && logoToolkit!=toolkit) {logoToolkit=toolkit;QueueLogo(*toolkit);}
}
ImU32 Design::Blend(ImU32 from,ImU32 to,float amount)
{
    const ImVec4 a=ImGui::ColorConvertU32ToFloat4(from),b=ImGui::ColorConvertU32ToFloat4(to);
    const float t=std::clamp(amount,0.0f,1.0f);
    return ImGui::GetColorU32({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t});
}
float Design::Fade(ImGuiID id,bool on,float speed)
{
    auto* storage=ImGui::GetStateStorage();
    const float before=storage->GetFloat(id,0.0f),target=on?1.0f:0.0f;
    const float value=before+(target-before)*std::min(1.0f,ImGui::GetIO().DeltaTime*speed);
    storage->SetFloat(id,std::abs(value-target)<0.002f?target:value);
    return value;
}
void Design::Icon(const char* name,ImVec2 position,float size,ImU32 tint)
{
    auto found=icons.find(name);if(found!=icons.end())found->second->Draw(*ImGui::GetWindowDrawList(),position,size,tint);
}
void Design::Image(const char* name,float size,ImU32 tint)
{
    Icon(name,ImGui::GetCursorScreenPos(),size,tint);ImGui::Dummy({size,size});
}
void Design::Logo(ImVec2 position,float size)
{
    const float pixels=size*ImGui::GetIO().DisplayFramebufferScale.x;
    size_t choice=LogoSizes.size()-1;
    for(size_t index=0;index<LogoSizes.size();++index)if(LogoSizes[index]>=pixels*0.9f){choice=index;break;}
    const ImTextureID texture=logoToolkit && logoImages[choice]?logoToolkit->ImageTexture(logoImages[choice]):ImTextureID{};
    auto* draw=ImGui::GetWindowDrawList();
    if(texture)draw->AddImage(texture,position,{position.x+size,position.y+size});
    else draw->AddCircleFilled({position.x+size*0.5f,position.y+size*0.5f},size*0.48f,IM_COL32(245,247,250,255));
}
void Design::LogoImage(float size)
{
    Logo(ImGui::GetCursorScreenPos(),size);ImGui::Dummy({size,size});
}
void Design::Tooltip(const char* text)
{
    if(text && text[0] && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))ImGui::SetTooltip("%s",text);
}
bool Design::BeginDialog(const char* titleId,float widthInFontUnits,bool* open)
{
    const auto* viewport=ImGui::GetMainViewport();
    const float width=std::min(ImGui::GetFontSize()*widthInFontUnits,viewport->WorkSize.x-ImGui::GetFontSize()*2);
    ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x*0.5f,viewport->WorkPos.y+viewport->WorkSize.y*0.5f},ImGuiCond_Always,{0.5f,0.5f});
    ImGui::SetNextWindowSizeConstraints({width,0},{width,viewport->WorkSize.y-ImGui::GetFontSize()*2});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{ImGui::GetFontSize()*1.1f,ImGui::GetFontSize()*0.9f});
    const bool visible=ImGui::BeginPopupModal(titleId,open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
    ImGui::PopStyleVar();
    return visible;
}
void Design::AlignRight(float width)
{
    const float target=ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-width;
    if(target>ImGui::GetCursorPosX())ImGui::SetCursorPosX(target);
}
float Design::ButtonWidth(const char* label,bool icon)
{
    const float unit=ImGui::GetFontSize(),padding=ImGui::GetStyle().FramePadding.x;
    return (icon?unit+padding*0.6f:0)+ImGui::CalcTextSize(label).x+padding*2.2f;
}
bool Design::Action(const char* id,const char* icon,const char* label,bool active,bool primary)
{
    const float unit=ImGui::GetFontSize(),height=ImGui::GetFrameHeight(),padding=ImGui::GetStyle().FramePadding.x;
    const bool hasIcon=icon && icon[0];
    const float width=label?(hasIcon?unit+padding*0.6f:0)+ImGui::CalcTextSize(label).x+padding*2.2f:height;
    const auto position=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton((std::string("##")+id).c_str(),{width,height});
    const bool hovered=ImGui::IsItemHovered(),held=ImGui::IsItemActive(),disabled=ImGui::GetItemFlags()&ImGuiItemFlags_Disabled;
    const float hover=Fade(ImGui::GetItemID(),hovered);
    const ImVec4 accent=Accent;
    ImU32 fill,text;
    if(primary) {
        const ImU32 base=ImGui::GetColorU32(accent),bright=ImGui::GetColorU32({std::min(1.0f,accent.x*1.15f),std::min(1.0f,accent.y*1.12f),std::min(1.0f,accent.z*1.06f),1.0f});
        fill=Blend(base,bright,hover);if(held)fill=ImGui::GetColorU32({accent.x*0.85f,accent.y*0.85f,accent.z*0.85f,1});
        text=IM_COL32(255,255,255,255);
    } else {
        const ImU32 base=active?ImGui::GetColorU32({accent.x,accent.y,accent.z,0.22f}):ImGui::GetColorU32(ImGuiCol_Button);
        fill=Blend(base,active?ImGui::GetColorU32({accent.x,accent.y,accent.z,0.32f}):ImGui::GetColorU32(ImGuiCol_ButtonHovered),hover);
        if(held)fill=ImGui::GetColorU32(ImGuiCol_ButtonActive);
        text=ImGui::GetColorU32(ImGuiCol_Text);
    }
    if(disabled){fill=WithAlpha(fill,0.5f);text=ImGui::GetColorU32(ImGuiCol_TextDisabled);}
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(position,{position.x+width,position.y+height},fill,ImGui::GetStyle().FrameRounding);
    const ImU32 iconColor=!primary && active && !disabled?ImGui::GetColorU32(accent):text;
    const float iconSize=unit*0.95f;
    if(label) {
        float x=position.x+padding*1.1f;
        if(hasIcon){Icon(icon,{x,position.y+(height-iconSize)*0.5f},iconSize,iconColor);x+=unit+padding*0.6f;}
        draw->AddText({x,position.y+(height-unit)*0.5f},text,label);
    } else if(hasIcon) {
        Icon(icon,{position.x+(width-iconSize)*0.5f,position.y+(height-iconSize)*0.5f},iconSize,iconColor);
        if(id[0]!='#')Tooltip(Tr(id));
    }
    return clicked;
}
bool Design::Ghost(const char* id,const char* icon,const char* label,bool active,float height)
{
    const float unit=ImGui::GetFontSize(),padding=ImGui::GetStyle().FramePadding.x;
    if(height<=0)height=ImGui::GetFrameHeight();
    const bool hasIcon=icon && icon[0];
    const float width=label?(hasIcon?unit+padding*0.5f:0)+ImGui::CalcTextSize(label).x+padding*2:height;
    const auto position=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton((std::string("##")+id).c_str(),{width,height});
    const bool hovered=ImGui::IsItemHovered(),held=ImGui::IsItemActive(),disabled=ImGui::GetItemFlags()&ImGuiItemFlags_Disabled;
    const float hover=Fade(ImGui::GetItemID(),hovered && !disabled);
    auto* draw=ImGui::GetWindowDrawList();
    const ImVec4 accent=Accent;
    ImU32 fill=active?ImGui::GetColorU32({accent.x,accent.y,accent.z,0.20f}):IM_COL32(255,255,255,0);
    fill=Blend(fill,active?ImGui::GetColorU32({accent.x,accent.y,accent.z,0.28f}):IM_COL32(255,255,255,20),hover);
    if(held && !disabled)fill=IM_COL32(255,255,255,12);
    if(fill>>IM_COL32_A_SHIFT)draw->AddRectFilled(position,{position.x+width,position.y+height},fill,ImGui::GetStyle().FrameRounding);
    const ImU32 text=disabled?ImGui::GetColorU32(ImGuiCol_TextDisabled):ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 iconColor=disabled?text:active?ImGui::GetColorU32(accent):Blend(ImGui::GetColorU32({0.78f,0.80f,0.84f,1}),text,hover);
    const float iconSize=unit*0.95f;
    if(label) {
        float x=position.x+padding;
        if(hasIcon){Icon(icon,{x,position.y+(height-iconSize)*0.5f},iconSize,iconColor);x+=unit+padding*0.5f;}
        draw->AddText({x,position.y+(height-unit)*0.5f},text,label);
    } else if(hasIcon) {
        Icon(icon,{position.x+(width-iconSize)*0.5f,position.y+(height-iconSize)*0.5f},iconSize,iconColor);
        if(id[0]!='#')Tooltip(Tr(id));
    }
    return clicked;
}
int Design::Segmented(const char* id,const Segment* segments,int count,int selected,float height)
{
    const float unit=ImGui::GetFontSize(),padding=ImGui::GetStyle().FramePadding.x,inset=std::max(2.0f,std::round(unit*0.14f));
    if(height<=0)height=ImGui::GetFrameHeight();
    std::vector<float> widths(count);float total=inset*2;
    for(int index=0;index<count;++index) {
        const bool hasLabel=segments[index].label && segments[index].label[0];
        widths[index]=hasLabel?unit+padding*0.55f+ImGui::CalcTextSize(segments[index].label).x+padding*2.2f:height;
        total+=widths[index];
    }
    const auto origin=ImGui::GetCursorScreenPos();
    auto* draw=ImGui::GetWindowDrawList();
    const float rounding=ImGui::GetStyle().FrameRounding;
    draw->AddRectFilled(origin,{origin.x+total,origin.y+height},ImGui::GetColorU32(ImGuiCol_FrameBg),rounding+inset);
    int clicked=-1;float x=origin.x+inset;
    ImGui::PushID(id);
    for(int index=0;index<count;++index) {
        ImGui::SetCursorScreenPos({x,origin.y+inset});
        const ImVec2 size{widths[index],height-inset*2};
        ImGui::PushID(index);
        if(ImGui::InvisibleButton("##segment",size))clicked=index;
        const bool hovered=ImGui::IsItemHovered();
        const float hover=Fade(ImGui::GetItemID(),hovered);
        const float chosen=Fade(ImGui::GetItemID()+1,index==selected,20.0f);
        if(chosen>0.01f)draw->AddRectFilled({x,origin.y+inset},{x+size.x,origin.y+inset+size.y},
            ImGui::GetColorU32({0.26f,0.27f,0.30f,chosen}),rounding);
        else if(hover>0.01f)draw->AddRectFilled({x,origin.y+inset},{x+size.x,origin.y+inset+size.y},IM_COL32(255,255,255,static_cast<int>(12*hover)),rounding);
        const ImU32 text=Blend(ImGui::GetColorU32({0.66f,0.68f,0.72f,1}),ImGui::GetColorU32(ImGuiCol_Text),std::max(chosen,hover));
        const ImU32 iconColor=Blend(text,ImGui::GetColorU32(Accent),chosen);
        const bool hasLabel=segments[index].label && segments[index].label[0];
        const float iconSize=unit*0.92f;
        if(hasLabel) {
            Icon(segments[index].icon,{x+padding*1.1f,origin.y+(height-iconSize)*0.5f},iconSize,iconColor);
            draw->AddText({x+padding*1.1f+unit+padding*0.55f,origin.y+(height-unit)*0.5f},text,segments[index].label);
        } else Icon(segments[index].icon,{x+(size.x-iconSize)*0.5f,origin.y+(height-iconSize)*0.5f},iconSize,iconColor);
        ImGui::PopID();
        x+=size.x;
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(origin);ImGui::Dummy({total,height});
    return clicked;
}
bool Design::CaptionButton(const char* id,CaptionGlyph glyph,ImVec2 size,bool dimmed)
{
    const auto position=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton(id,size);
    const bool hovered=ImGui::IsItemHovered(),held=ImGui::IsItemActive();
    const float hover=Fade(ImGui::GetItemID(),hovered,22.0f);
    auto* draw=ImGui::GetWindowDrawList();
    const bool close=glyph==CaptionGlyph::Close;
    ImU32 glyphColor=dimmed?IM_COL32(128,130,136,255):IM_COL32(232,234,238,255);
    if(close) {
        const ImU32 red=held?IM_COL32(200,64,52,255):IM_COL32(196,43,28,255);
        if(hover>0.01f)draw->AddRectFilled(position,{position.x+size.x,position.y+size.y},WithAlpha(red,hover));
        glyphColor=Blend(glyphColor,IM_COL32(255,255,255,255),hover);
    } else if(hover>0.01f) {
        draw->AddRectFilled(position,{position.x+size.x,position.y+size.y},IM_COL32(255,255,255,static_cast<int>((held?10:18)*hover)));
        glyphColor=Blend(glyphColor,IM_COL32(255,255,255,255),hover);
    }
    DrawCaptionGlyph(*draw,glyph,{position.x+size.x*0.5f,position.y+size.y*0.5f},ImGui::GetFontSize(),glyphColor);
    return clicked;
}
void Design::Spinner(float radius,ImU32 color)
{
    const auto position=ImGui::GetCursorScreenPos();
    const ImVec2 center{position.x+radius,position.y+radius};
    const float start=static_cast<float>(ImGui::GetTime())*6.0f;
    auto* draw=ImGui::GetWindowDrawList();
    draw->PathArcTo(center,radius*0.8f,start,start+4.2f,24);
    draw->PathStroke(color,0,std::max(1.5f,radius*0.25f));
    ImGui::Dummy({radius*2,radius*2});
}
void Design::Heading(const char* text)
{
    auto& fonts=ImGui::GetIO().Fonts->Fonts;
    if(fonts.Size>2)ImGui::PushFont(fonts[2]);ImGui::TextUnformatted(text);if(fonts.Size>2)ImGui::PopFont();
}
void Design::Eyebrow(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text,Muted);
    ImGui::SetWindowFontScale(0.86f);ImGui::TextUnformatted(text);ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
}
void Design::Badge(const char* text,ImVec4 color)
{
    const float unit=ImGui::GetFontSize();
    auto position=ImGui::GetCursorScreenPos();auto size=ImGui::CalcTextSize(text);size.x+=unit*0.8f;size.y+=unit*0.3f;
    ImGui::GetWindowDrawList()->AddRectFilled(position,{position.x+size.x,position.y+size.y},ImGui::GetColorU32({color.x,color.y,color.z,0.16f}),size.y*0.5f);
    ImGui::GetWindowDrawList()->AddText({position.x+unit*0.4f,position.y+unit*0.15f},ImGui::GetColorU32(color),text);ImGui::Dummy(size);
}
}
