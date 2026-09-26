// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CSvgIcon.h"
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#include <algorithm>
#include <vector>

namespace Concord {
struct SvgIcon::Impl {NSVGimage* image=nullptr;~Impl(){nsvgDelete(image);}};
SvgIcon::SvgIcon():m_impl(std::make_unique<Impl>()){}
SvgIcon::~SvgIcon()=default;
bool SvgIcon::Load(const std::string& source)
{
    if(source.size()>1024*1024)return false;
    std::vector<char> data(source.begin(),source.end());data.push_back(0);
    auto* parsed=nsvgParse(data.data(),"px",96);
    if(!parsed || parsed->width<=0 || parsed->height<=0){nsvgDelete(parsed);return false;}
    nsvgDelete(m_impl->image);m_impl->image=parsed;return true;
}
void SvgIcon::Draw(ImDrawList& draw,ImVec2 position,float size,ImU32 tint) const
{
    if(!m_impl->image)return;
    const auto& image=*m_impl->image;const float scale=size/std::max(image.width,image.height);
    position.x+=(size-image.width*scale)/2;position.y+=(size-image.height*scale)/2;
    auto point=[&](const float* value){return ImVec2{position.x+value[0]*scale,position.y+value[1]*scale};};
    for(auto* shape=image.shapes;shape;shape=shape->next) {
        if(!(shape->flags&NSVG_FLAGS_VISIBLE))continue;
        const auto color=(tint&~IM_COL32_A_MASK)|(static_cast<ImU32>(((tint>>IM_COL32_A_SHIFT)&255)*shape->opacity)<<IM_COL32_A_SHIFT);
        for(auto* path=shape->paths;path;path=path->next) {
            auto trace=[&] {
                draw.PathClear();draw.PathLineTo(point(path->pts));
                for(int i=1;i<path->npts;i+=3)draw.PathBezierCubicCurveTo(point(&path->pts[i*2]),point(&path->pts[(i+1)*2]),point(&path->pts[(i+2)*2]));
            };
            if(shape->fill.type==NSVG_PAINT_COLOR){trace();draw.PathFillConcave(color);}
            if(shape->stroke.type==NSVG_PAINT_COLOR){trace();draw.PathStroke(color,path->closed?ImDrawFlags_Closed:0,std::max(1.0f,shape->strokeWidth*scale));}
        }
    }
}
}
