// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include "editor/PropertyGrid.h"
#include <Concord/CCamera.h>
#include <Concord/CLight.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace Concord::Editor {
namespace {
float* Data(Mat4& matrix) {return &matrix.col[0].x;}
Vec3 Column(const Mat4& matrix,int index) {return {matrix.col[index].x,matrix.col[index].y,matrix.col[index].z};}
/** ImGuizmo produces a matrix; recover the engine's Y-X-Z Euler convention. */
Transform Decompose(const Mat4& matrix)
{
    Transform result;result.position=Column(matrix,3);
    Vec3 columns[]={Column(matrix,0),Column(matrix,1),Column(matrix,2)};
    float lengths[]={std::sqrt(Dot(columns[0],columns[0])),std::sqrt(Dot(columns[1],columns[1])),std::sqrt(Dot(columns[2],columns[2]))};
    for(int i=0;i<3;++i)columns[i]=columns[i]*(1.0f/std::max(lengths[i],0.001f));
    result.scale={std::max(lengths[0],0.001f),std::max(lengths[1],0.001f),std::max(lengths[2],0.001f)};
    const float pitch=std::asin(std::clamp(-columns[2].y,-1.0f,1.0f));
    float yaw=0,roll=0;
    if(std::abs(std::cos(pitch))>0.0001f) {yaw=std::atan2(columns[2].x,columns[2].z);roll=std::atan2(columns[0].y,columns[1].y);}
    else yaw=std::atan2(-columns[0].z,columns[0].x);
    result.rotation={Degrees(pitch),Degrees(yaw),Degrees(roll)};return result;
}
/** Screen projection shared by the grid and scene gizmos; false behind the camera. */
struct Projector {
    Mat4 viewProjection;
    ImVec2 origin,size;
    bool operator()(Vec3 point,ImVec2& screen) const
    {
        const auto clip=viewProjection*Vec4{point.x,point.y,point.z,1};
        if(clip.w<0.05f)return false;
        screen={origin.x+(clip.x/clip.w*0.5f+0.5f)*size.x,origin.y+(0.5f-clip.y/clip.w*0.5f)*size.y};
        return true;
    }
    void Line(ImDrawList& draw,Vec3 a,Vec3 b,ImU32 color,float thickness=1.0f) const
    {
        Vec4 ca=viewProjection*Vec4{a.x,a.y,a.z,1},cb=viewProjection*Vec4{b.x,b.y,b.z,1};
        constexpr float kNear=0.05f;
        const bool aIn=ca.w>=kNear,bIn=cb.w>=kNear;
        if(!aIn && !bIn)return;
        if(aIn!=bIn) {
            const float denom=cb.w-ca.w;
            if(std::abs(denom)<1e-6f)return;
            const float t=(kNear-ca.w)/denom;
            if(t<0.0f || t>1.0f)return;
            const Vec4 hit{ca.x+(cb.x-ca.x)*t,ca.y+(cb.y-ca.y)*t,ca.z+(cb.z-ca.z)*t,kNear};
            if(aIn)cb=hit;else ca=hit;
        }
        auto toScreen=[&](const Vec4& clip) {
            return ImVec2{origin.x+(clip.x/clip.w*0.5f+0.5f)*size.x,origin.y+(0.5f-clip.y/clip.w*0.5f)*size.y};
        };
        ImVec2 sa=toScreen(ca),sb=toScreen(cb);
        if(!std::isfinite(sa.x) || !std::isfinite(sa.y) || !std::isfinite(sb.x) || !std::isfinite(sb.y))return;
        const ImVec2 lo{origin.x-size.x,origin.y-size.y},hi{origin.x+size.x*2.0f,origin.y+size.y*2.0f};
        auto code=[&](ImVec2 p) {
            int bits=0;
            if(p.x<lo.x)bits|=1;else if(p.x>hi.x)bits|=2;
            if(p.y<lo.y)bits|=4;else if(p.y>hi.y)bits|=8;
            return bits;
        };
        int caBits=code(sa),cbBits=code(sb);
        for(int guard=0;guard<8 && (caBits|cbBits);++guard) {
            if(caBits&cbBits)return;
            const int out=caBits?caBits:cbBits;
            const float dx=sb.x-sa.x,dy=sb.y-sa.y;
            ImVec2 hit=sa;
            if((out&3) && dx==0.0f)return;
            if((out&12) && dy==0.0f)return;
            if(out&1){hit.y=sa.y+dy*(lo.x-sa.x)/dx;hit.x=lo.x;}
            else if(out&2){hit.y=sa.y+dy*(hi.x-sa.x)/dx;hit.x=hi.x;}
            else if(out&4){hit.x=sa.x+dx*(lo.y-sa.y)/dy;hit.y=lo.y;}
            else {hit.x=sa.x+dx*(hi.y-sa.y)/dy;hit.y=hi.y;}
            if(out==caBits){sa=hit;caBits=code(sa);}else{sb=hit;cbBits=code(sb);}
        }
        if(caBits|cbBits)return;
        // AddLine itself shifts by half a pixel, so integer points land on pixel centers.
        sa.x=std::round(sa.x);sa.y=std::round(sa.y);sb.x=std::round(sb.x);sb.y=std::round(sb.y);
        if(sa.x!=sb.x || sa.y!=sb.y)draw.AddLine(sa,sb,color,thickness);
    }
};
struct GameCameraGeometry {
    Vec3 position{};
    Vec3 corners[4]{};
    Vec3 capLeft{},capRight{},cap{};
};
GameCameraGeometry BuildGameCamera(const SceneCameraSettings& camera,float viewDistance,int pixelWidth,int pixelHeight)
{
    GameCameraGeometry geometry;
    geometry.position=camera.position;
    Vec3 forward=camera.target-camera.position;
    forward=Dot(forward,forward)<1e-8f?Vec3{0,0,-1}:Normalize(forward);
    Vec3 right=Cross(forward,{0,1,0});
    right=Dot(right,right)<1e-6f?Vec3{1,0,0}:Normalize(right);
    const Vec3 up=Cross(right,forward);
    const float depth=std::clamp(viewDistance*0.12f,0.8f,6.0f);
    const float aspect=static_cast<float>(std::max(1,pixelWidth))/static_cast<float>(std::max(1,pixelHeight));
    const float halfHeight=camera.orthographic?camera.orthographicSize*0.5f:std::tan(Radians(camera.fovYDegrees*0.5f))*depth;
    const float halfWidth=halfHeight*aspect;
    const Vec3 center=camera.position+forward*depth;
    geometry.corners[0]=center+right*halfWidth+up*halfHeight;
    geometry.corners[1]=center-right*halfWidth+up*halfHeight;
    geometry.corners[2]=center-right*halfWidth-up*halfHeight;
    geometry.corners[3]=center+right*halfWidth-up*halfHeight;
    geometry.capLeft=geometry.corners[0]+up*halfHeight*0.15f;
    geometry.capRight=geometry.corners[1]+up*halfHeight*0.15f;
    geometry.cap=center+up*halfHeight*1.45f;
    return geometry;
}
Vec3 SunAnchor(Vec3 target,float distance)
{
    return target+Vec3{0,std::clamp(distance*0.28f,2.0f,12.0f),0};
}
Mat4 BasisAt(Vec3 origin,Vec3 forward)
{
    forward=Normalize(forward);
    if(Dot(forward,forward)<0.5f)forward={0,0,-1};
    const Vec3 helper=std::abs(forward.y)>0.92f?Vec3{1,0,0}:Vec3{0,1,0};
    const Vec3 right=Normalize(Cross(helper,forward));
    const Vec3 up=Cross(forward,right);
    Mat4 matrix=Mat4::Identity();
    matrix.col[0]={right.x,right.y,right.z,0};
    matrix.col[1]={up.x,up.y,up.z,0};
    matrix.col[2]={forward.x,forward.y,forward.z,0};
    matrix.col[3]={origin.x,origin.y,origin.z,1};
    return matrix;
}
float PointDistance(ImVec2 a,ImVec2 b)
{
    const float dx=a.x-b.x,dy=a.y-b.y;return std::sqrt(dx*dx+dy*dy);
}
float SegmentDistance(ImVec2 point,ImVec2 a,ImVec2 b)
{
    const float abx=b.x-a.x,aby=b.y-a.y,length=abx*abx+aby*aby;
    const float t=length<1e-4f?0.0f:std::clamp(((point.x-a.x)*abx+(point.y-a.y)*aby)/length,0.0f,1.0f);
    return PointDistance(point,{a.x+abx*t,a.y+aby*t});
}
float SegmentHit(const Projector& project,ImVec2 mouse,Vec3 a,Vec3 b)
{
    ImVec2 sa,sb;if(!project(a,sa) || !project(b,sb))return std::numeric_limits<float>::max();
    return SegmentDistance(mouse,sa,sb);
}
}
void Workspace::FocusSelection()
{
    if(m_worldSelection==WorldSelection::Camera) {
        m_target=m_document.camera.position;m_distance=std::clamp(8.0f,2.0f,200.0f);return;
    }
    if(m_worldSelection==WorldSelection::Sun) {
        m_target=m_document.camera.target;m_distance=std::clamp(m_distance,6.0f,80.0f);return;
    }
    if(m_worldSelection==WorldSelection::Sky || m_worldSelection==WorldSelection::Clouds) {
        m_pitch=m_worldSelection==WorldSelection::Clouds?0.85f:0.55f;return;
    }
    if(!IsSelected())return;
    const auto& object=m_document.objects[m_selection];m_target=object.transform.position;
    m_distance=std::max({object.size.x*object.transform.scale.x,object.size.y*object.transform.scale.y,object.size.z*object.transform.scale.z})*2.5f;
    m_distance=std::clamp(m_distance,2.0f,200.0f);
}
void Workspace::FrameAll()
{
    Vec3 low{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},high=low*-1.0f;
    bool any=false;
    for(const auto& object:m_document.objects) {
        if(!object.visible)continue;
        const Vec3 extent{object.size.x*object.transform.scale.x*0.5f,object.size.y*object.transform.scale.y*0.5f,object.size.z*object.transform.scale.z*0.5f};
        const float radius=std::sqrt(Dot(extent,extent));
        const auto p=object.transform.position;
        low={std::min(low.x,p.x-radius),std::min(low.y,p.y-radius),std::min(low.z,p.z-radius)};
        high={std::max(high.x,p.x+radius),std::max(high.y,p.y+radius),std::max(high.z,p.z+radius)};
        any=true;
    }
    if(!any){m_target={0,1,0};m_distance=15;return;}
    m_target=(low+high)*0.5f;
    const Vec3 half=(high-low)*0.5f;
    m_distance=std::clamp(std::sqrt(Dot(half,half))*2.1f,3.0f,400.0f);
}
void Workspace::UpdateCamera()
{
    m_eye=m_target+Vec3{std::sin(m_yaw)*std::cos(m_pitch),std::sin(m_pitch),std::cos(m_yaw)*std::cos(m_pitch)}*m_distance;
    auto* transform=m_camera.Get<Transform>();transform->position=m_eye;transform->rotation=Object::LookAtRotation(m_eye,m_target);
}
void Workspace::Pick(ImVec2 origin,ImVec2 size)
{
    const auto mouse=ImGui::GetMousePos();
    const auto forward=Normalize(m_target-m_eye);const auto right=Normalize(Cross(forward,{0,1,0}));const auto up=Cross(right,forward);
    const float x=(2*(mouse.x-origin.x)/size.x-1)*size.x/size.y*std::tan(Radians(30.0f));
    const float y=(1-2*(mouse.y-origin.y)/size.y)*std::tan(Radians(30.0f));
    const auto ray=Normalize(forward+right*x+up*y);float closest=std::numeric_limits<float>::max();int objectHit=-1;
    for(int i=0;i<static_cast<int>(m_document.objects.size());++i) {
        const auto& object=m_document.objects[i];if(!object.visible)continue;auto matrix=object.transform.ToMatrix();
        const Vec3 axes[]={Column(matrix,0),Column(matrix,1),Column(matrix,2)};
        const float extents[]={object.size.x*0.5f,object.size.y*0.5f,object.size.z*0.5f};
        float nearHit=0,farHit=closest;bool hit=true;
        for(int axis=0;axis<3;++axis) {
            float square=Dot(axes[axis],axes[axis]);
            float o=Dot(m_eye-object.transform.position,axes[axis])/square,d=Dot(ray,axes[axis])/square;
            if(std::abs(d)<1e-7f) {if(std::abs(o)>extents[axis])hit=false;continue;}
            float a=(-extents[axis]-o)/d,b=(extents[axis]-o)/d;if(a>b)std::swap(a,b);
            nearHit=std::max(nearHit,a);farHit=std::min(farHit,b);if(nearHit>farHit)hit=false;
        }
        if(hit && nearHit<closest) {closest=nearHit;objectHit=i;}
    }
    WorldSelection gizmo=WorldSelection::None;float gizmoDistance=std::numeric_limits<float>::max();
    if(m_showGameCamera) {
        auto view=Mat4::LookAt(m_eye,m_target,{0,1,0});auto projection=Mat4::Perspective(Radians(60.0f),size.x/size.y,0.1f,1000);
        projection.col[1].y=-projection.col[1].y;
        const Projector project{projection*view,origin,size};
        const auto camera=BuildGameCamera(m_document.camera,m_distance,m_projectConfig.width,m_projectConfig.height);
        float cameraDistance=std::numeric_limits<float>::max();
        ImVec2 screen;
        if(project(camera.position,screen))cameraDistance=std::min(cameraDistance,PointDistance(mouse,screen));
        for(int index=0;index<4;++index) {
            cameraDistance=std::min(cameraDistance,SegmentHit(project,mouse,camera.position,camera.corners[index]));
            cameraDistance=std::min(cameraDistance,SegmentHit(project,mouse,camera.corners[index],camera.corners[(index+1)%4]));
        }
        cameraDistance=std::min(cameraDistance,SegmentHit(project,mouse,camera.capLeft,camera.cap));
        cameraDistance=std::min(cameraDistance,SegmentHit(project,mouse,camera.capRight,camera.cap));
        const float slop=std::max(12.0f,ImGui::GetFontSize()*0.8f);
        if(cameraDistance<=slop){gizmo=WorldSelection::Camera;gizmoDistance=cameraDistance;}
        const Vec3 anchor=SunAnchor(m_target,m_distance);
        if(project(anchor,screen)) {
            const float sunDistance=PointDistance(mouse,screen);
            if(sunDistance<=slop*1.35f && sunDistance<gizmoDistance){gizmo=WorldSelection::Sun;gizmoDistance=sunDistance;}
        }
        LightComponent light{};light.elevationDegrees=m_document.sun.elevationDegrees;light.azimuthDegrees=m_document.sun.azimuthDegrees;
        const float arrow=SegmentHit(project,mouse,anchor,anchor+Object::LightDirection(light)*std::clamp(m_distance*0.1f,0.8f,4.0f));
        if(arrow<=slop && arrow<gizmoDistance)gizmo=WorldSelection::Sun;
    }
    if(gizmo!=WorldSelection::None)SelectWorld(gizmo);
    else {m_selection=objectHit;m_worldSelection=WorldSelection::None;}
}
void Workspace::DrawSceneGizmos(ImDrawList& draw,const Mat4& view,const Mat4& projection,ImVec2 origin,ImVec2 size)
{
    const Projector project{projection*view,origin,size};
    if(m_showGrid) {
        const float pixelsPerUnit=size.y/(2.0f*std::max(m_distance,0.25f)*std::tan(Radians(30.0f)));
        int step=10;
        if(pixelsPerUnit<1.5f)step=50;
        if(pixelsPerUnit<0.3f)step=200;
        const bool fine=pixelsPerUnit>=6.0f;
        if(fine)step=1;
        const int accent=fine?10:step*5;
        const int cover=std::clamp(static_cast<int>(std::ceil(m_distance*6.0f)),80,fine?120:800);
        const int half=std::clamp((cover+step-1)/step,8,fine?120:60);
        const int snap=fine?accent:step;
        const int originX=static_cast<int>(std::round(m_target.x/static_cast<float>(snap)))*snap;
        const int originZ=static_cast<int>(std::round(m_target.z/static_cast<float>(snap)))*snap;
        const int span=half*step;
        const auto paint=[&](int x0,int z0,int x1,int z1,ImU32 color,float width) {
            project.Line(draw,{static_cast<float>(x0),0,static_cast<float>(z0)},{static_cast<float>(x1),0,static_cast<float>(z1)},color,width);
        };
        for(int i=-half;i<=half;++i) {
            const int x=originX+i*step,z=originZ+i*step;
            const bool majorX=x%accent==0,majorZ=z%accent==0;
            const int lineAlpha=majorX?96:(fine?36:52);
            const int lineAlphaZ=majorZ?96:(fine?36:52);
            const ImU32 colorX=x==0?IM_COL32(92,146,232,180):IM_COL32(170,178,194,lineAlpha);
            const ImU32 colorZ=z==0?IM_COL32(226,88,88,180):IM_COL32(170,178,194,lineAlphaZ);
            paint(x,originZ-span,x,originZ+span,colorX,x==0?1.6f:1.0f);
            paint(originX-span,z,originX+span,z,colorZ,z==0?1.6f:1.0f);
        }
    }
    if(!m_showGameCamera)return;
    const auto camera=BuildGameCamera(m_document.camera,m_distance,m_projectConfig.width,m_projectConfig.height);
    const bool cameraSelected=m_worldSelection==WorldSelection::Camera;
    const ImU32 cameraColor=cameraSelected?IM_COL32(110,186,255,245):IM_COL32(214,214,214,220);
    const float cameraWidth=cameraSelected?2.2f:1.4f;
    for(int index=0;index<4;++index) {
        project.Line(draw,camera.position,camera.corners[index],cameraColor,cameraWidth);
        project.Line(draw,camera.corners[index],camera.corners[(index+1)%4],cameraColor,cameraWidth);
    }
    project.Line(draw,camera.capLeft,camera.cap,cameraColor,cameraWidth);
    project.Line(draw,camera.capRight,camera.cap,cameraColor,cameraWidth);
    ImVec2 label;
    if(project(camera.position,label))draw.AddText({label.x+6,label.y-ImGui::GetFontSize()-2},cameraColor,Tr("Game camera"));
    LightComponent light{};light.elevationDegrees=m_document.sun.elevationDegrees;light.azimuthDegrees=m_document.sun.azimuthDegrees;
    const Vec3 direction=Object::LightDirection(light);
    const Vec3 anchor=SunAnchor(m_target,m_distance);
    const bool sunSelected=m_worldSelection==WorldSelection::Sun;
    ImVec2 sun;
    if(project(anchor,sun)) {
        const ImU32 sunColor=sunSelected?IM_COL32(110,186,255,245):IM_COL32(250,196,90,230);const float radius=ImGui::GetFontSize()*0.35f;
        draw.AddCircleFilled(sun,radius,sunColor);
        if(sunSelected)draw.AddCircle(sun,radius*2.3f,sunColor,24,2.0f);
        for(int ray=0;ray<8;++ray) {
            const float angle=ray*std::numbers::pi_v<float>*0.25f;
            draw.AddLine({sun.x+std::cos(angle)*radius*1.4f,sun.y+std::sin(angle)*radius*1.4f},{sun.x+std::cos(angle)*radius*2.0f,sun.y+std::sin(angle)*radius*2.0f},sunColor,1.5f);
        }
        const float length=std::clamp(m_distance*0.1f,0.8f,4.0f);
        ImVec2 tip;
        if(project(anchor+direction*length,tip)) {
            draw.AddLine(sun,tip,sunColor,2.0f);
            const ImVec2 axis{tip.x-sun.x,tip.y-sun.y};const float norm=std::max(1.0f,std::sqrt(axis.x*axis.x+axis.y*axis.y));
            const ImVec2 unit{axis.x/norm,axis.y/norm},side{-unit.y,unit.x};const float head=radius*1.3f;
            draw.AddTriangleFilled(tip,{tip.x-unit.x*head+side.x*head*0.55f,tip.y-unit.y*head+side.y*head*0.55f},
                                   {tip.x-unit.x*head-side.x*head*0.55f,tip.y-unit.y*head-side.y*head*0.55f},sunColor);
        }
    }
}
void Workspace::ViewportOverlay(ImVec2 origin,ImVec2 size)
{
    const float unit=ImGui::GetFontSize(),frame=ImGui::GetFrameHeight(),margin=std::round(unit*0.55f);
    ImVec4 pill=ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);pill.w=0.94f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,pill);
    ImGui::PushStyleColor(ImGuiCol_Border,ImVec4{1,1,1,0.12f});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,ImGui::GetStyle().FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,1);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{unit*0.28f,unit*0.18f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{unit*0.1f,0});
    const auto flags=ImGuiChildFlags_AutoResizeX|ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_Borders;
    const bool labels=size.x>unit*40;
    ImGui::SetCursorScreenPos({origin.x+margin,origin.y+margin});
    if(ImGui::BeginChild("##transformTools",{0,0},flags,ImGuiWindowFlags_NoScrollbar)) {
        if(Design::Ghost("Move (W)","move",labels?Tr("Move"):nullptr,m_operation==ImGuizmo::TRANSLATE))m_operation=ImGuizmo::TRANSLATE;ImGui::SameLine();
        if(Design::Ghost("Rotate (E)","rotate",labels?Tr("Rotate"):nullptr,m_operation==ImGuizmo::ROTATE))m_operation=ImGuizmo::ROTATE;ImGui::SameLine();
        if(Design::Ghost("Scale (R)","scale",labels?Tr("Scale"):nullptr,m_operation==ImGuizmo::SCALE))m_operation=ImGuizmo::SCALE;ImGui::SameLine();
        ImGui::Dummy({unit*0.3f,frame});ImGui::SameLine();
        if(Design::Ghost(m_local?"Local space: axes follow the object":"World space: axes follow the scene","",m_local?Tr("Local"):Tr("World")))m_local=!m_local;
        Design::Tooltip(m_local?Tr("Local space: axes follow the object"):Tr("World space: axes follow the scene"));
        ImGui::SameLine();
        if(Design::Ghost("Snap to steps","grid",labels?Tr("Snap"):nullptr,m_snap))m_snap=!m_snap;
        ImGui::SameLine();
        if(Design::Ghost("Snap steps","chevron"))ImGui::OpenPopup("##snapSteps");
        if(ImGui::BeginPopup("##snapSteps")) {
            ImGui::TextDisabled("%s",Tr("Snap steps"));
            if(PropertyGrid::Begin("##steps")) {
                PropertyGrid::Float("Move",m_snapTranslate,0.05f,0.01f,100,"%.2f");
                PropertyGrid::Float("Rotate",m_snapRotate,0.5f,1,180,"%.0f");
                PropertyGrid::Float("Scale",m_snapScale,0.01f,0.01f,10,"%.2f");
                PropertyGrid::End();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    const float viewWidth=frame*4+unit*0.12f*3+unit*0.4f;
    ImGui::SetCursorScreenPos({origin.x+size.x-margin-viewWidth,origin.y+size.y-margin-frame});
    if(ImGui::BeginChild("##viewTools",{0,0},flags,ImGuiWindowFlags_NoScrollbar)) {
        if(Design::Ghost("Show grid","grid",nullptr,m_showGrid))m_showGrid=!m_showGrid;ImGui::SameLine();
        if(Design::Ghost("Show game camera and sun","eye",nullptr,m_showGameCamera))m_showGameCamera=!m_showGameCamera;ImGui::SameLine();
        if(Design::Ghost("Frame all objects (Home)","focus"))FrameAll();ImGui::SameLine();
        if(Design::Ghost("Camera views","more"))ImGui::OpenPopup("##cameraViews");
        if(ImGui::BeginPopup("##cameraViews")) {
            constexpr float quarter=std::numbers::pi_v<float>*0.5f;
            if(ImGui::MenuItem(Tr("Perspective"))){m_yaw=0.68573f;m_pitch=0.399f;}
            ImGui::Separator();
            if(ImGui::MenuItem(Tr("Front"))){m_yaw=0;m_pitch=0;}
            if(ImGui::MenuItem(Tr("Back"))){m_yaw=quarter*2;m_pitch=0;}
            if(ImGui::MenuItem(Tr("Right"))){m_yaw=quarter;m_pitch=0;}
            if(ImGui::MenuItem(Tr("Left"))){m_yaw=-quarter;m_pitch=0;}
            if(ImGui::MenuItem(Tr("Top"))){m_yaw=0;m_pitch=1.45f;}
            if(ImGui::MenuItem(Tr("Bottom"))){m_yaw=0;m_pitch=-1.45f;}
            ImGui::Separator();
            if(ImGui::MenuItem(Tr("Move game camera here"))){Checkpoint();m_document.camera.position=m_eye;m_document.camera.target=m_target;Synchronize();}
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(4);ImGui::PopStyleColor(2);
}
void Workspace::ViewportContextMenu()
{
    if(!ImGui::BeginPopup("##viewportMenu"))return;
    if(ImGui::BeginMenu(Tr("Add object"))){AddObjectMenu();ImGui::EndMenu();}
    ImGui::Separator();
    if(ImGui::MenuItem(Tr("Copy"),"Ctrl+C",false,IsSelected()))CopySelection();
    if(ImGui::MenuItem(Tr("Paste"),"Ctrl+V",false,m_copiedObject.has_value()))PasteObject();
    if(ImGui::MenuItem(Tr("Duplicate"),"Ctrl+D",false,IsSelected()))DuplicateSelection();
    if(ImGui::MenuItem(Tr("Rename"),"F2",false,IsSelected()))BeginRename(m_selection);
    if(ImGui::MenuItem(Tr("Delete"),"Del",false,IsSelected()))DeleteSelection();
    ImGui::Separator();
    if(ImGui::MenuItem(Tr("Focus selection"),"F",false,IsSelected() || m_worldSelection!=WorldSelection::None))FocusSelection();
    if(ImGui::MenuItem(Tr("Frame all objects"),"Home"))FrameAll();
    ImGui::EndPopup();
}
void Workspace::Viewport()
{
    ImGuizmo::BeginFrame();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    const bool open=ImGui::Begin(TrId("Scene").c_str(),nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if(open) {
        const auto origin=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();size.x=std::max(size.x,32.0f);size.y=std::max(size.y,32.0f);
        auto& ui=*m_game.Toolkit();const auto framebufferScale=ImGui::GetIO().DisplayFramebufferScale;
        ui.SetSceneViewport(static_cast<u32>(size.x*framebufferScale.x),static_cast<u32>(size.y*framebufferScale.y));
        ImGui::Image(ui.SceneTexture(),size);
        const bool hovered=ImGui::IsItemHovered();
        auto& io=ImGui::GetIO();
        const float unit=ImGui::GetFontSize();
        if(hovered && !ImGuizmo::IsUsing()) {
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)){m_orbiting=true;m_rightPress=io.MousePos;}
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))m_panning=true;
            if(io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))m_orbiting=true;
            m_distance=std::clamp(m_distance*std::pow(0.87f,io.MouseWheel),0.3f,500.0f);
        }
        if(m_orbiting && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !(io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
            const float dx=io.MousePos.x-m_rightPress.x,dy=io.MousePos.y-m_rightPress.y;
            if(ImGui::IsMouseReleased(ImGuiMouseButton_Right) && dx*dx+dy*dy<16 && hovered){m_contextMenu=true;}
            m_orbiting=false;
        }
        if(m_panning && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))m_panning=false;
        if(m_orbiting) {
            m_yaw-=io.MouseDelta.x*0.0065f;m_pitch=std::clamp(m_pitch+io.MouseDelta.y*0.0065f,-1.45f,1.45f);
            const Vec3 forward=Normalize(m_target-m_eye),right=Normalize(Cross(forward,{0,1,0}));
            Vec3 move{};
            if(ImGui::IsKeyDown(ImGuiKey_W))move=move+forward;
            if(ImGui::IsKeyDown(ImGuiKey_S))move=move-forward;
            if(ImGui::IsKeyDown(ImGuiKey_D))move=move+right;
            if(ImGui::IsKeyDown(ImGuiKey_A))move=move-right;
            if(ImGui::IsKeyDown(ImGuiKey_E))move=move+Vec3{0,1,0};
            if(ImGui::IsKeyDown(ImGuiKey_Q))move=move-Vec3{0,1,0};
            if(Dot(move,move)>0) {
                const float speed=std::max(2.0f,m_distance*0.9f)*(io.KeyShift?3.0f:1.0f);
                m_target=m_target+Normalize(move)*(speed*io.DeltaTime);
            }
        }
        if(m_panning) {
            auto forward=Normalize(m_target-m_eye),right=Normalize(Cross(forward,{0,1,0})),up=Cross(right,forward);
            m_target=m_target+right*(-io.MouseDelta.x*m_distance*0.0015f)+up*(io.MouseDelta.y*m_distance*0.0015f);
        }
        if(hovered && !io.WantTextInput && !m_orbiting) {
            if(ImGui::IsKeyPressed(ImGuiKey_F,false))FocusSelection();
            if(!io.KeyCtrl) {
                if(ImGui::IsKeyPressed(ImGuiKey_W,false))m_operation=ImGuizmo::TRANSLATE;
                if(ImGui::IsKeyPressed(ImGuiKey_E,false))m_operation=ImGuizmo::ROTATE;
                if(ImGui::IsKeyPressed(ImGuiKey_R,false))m_operation=ImGuizmo::SCALE;
            }
        }
        UpdateCamera();
        // The renderer uses the actual per-frame texture extent; avoid resize-frame aspect stretching.
        m_camera.Get<CameraComponent>()->aspectOverride=size.x/size.y;
        auto view=Mat4::LookAt(m_eye,m_target,{0,1,0});auto projection=Mat4::Perspective(Radians(60.0f),size.x/size.y,0.1f,1000);
        projection.col[1].y=-projection.col[1].y;
        auto* draw=ImGui::GetWindowDrawList();
        ImGuizmo::SetOrthographic(false);ImGuizmo::SetDrawlist(draw);ImGuizmo::SetRect(origin.x,origin.y,size.x,size.y);
        draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
        DrawSceneGizmos(*draw,view,projection,origin,size);
        const bool worldGizmo=m_showGameCamera && (m_worldSelection==WorldSelection::Camera || m_worldSelection==WorldSelection::Sun);
        if(IsSelected()) {
            auto& object=m_document.objects[m_selection];auto matrix=object.transform.ToMatrix();
            const float step=m_operation==ImGuizmo::ROTATE?m_snapRotate:m_operation==ImGuizmo::SCALE?m_snapScale:m_snapTranslate;
            float snap[]={step,step,step};
            const bool snapping=m_snap!=io.KeyCtrl;
            auto before=m_snapshot;
            const bool manipulated=ImGuizmo::Manipulate(Data(view),Data(projection),m_operation,m_local?ImGuizmo::LOCAL:ImGuizmo::WORLD,Data(matrix),nullptr,snapping?snap:nullptr);
            const bool usingNow=ImGuizmo::IsUsing();
            if(usingNow && !m_gizmoWasUsing) {m_undo.push_back(before);if(m_undo.size()>64)m_undo.pop_front();m_redo.clear();}
            if(manipulated) {object.transform=Decompose(matrix);m_sceneDirty=true;Synchronize();}
            m_gizmoWasUsing=usingNow;
            const ImU32 outline=ImGui::GetColorU32({Design::Accent.x,Design::Accent.y,Design::Accent.z,0.95f});
            const Projector project{projection*view,origin,size};
            auto cornerAt=[&](int index) {
                const Vec4 local{(index&1?0.5f:-0.5f)*object.size.x,(index&2?0.5f:-0.5f)*object.size.y,(index&4?0.5f:-0.5f)*object.size.z,1};
                const Vec4 world=matrix*local;
                return Vec3{world.x,world.y,world.z};
            };
            for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))project.Line(*draw,cornerAt(i),cornerAt(i|bit),outline,1.6f);
        } else if(worldGizmo) {
            const bool sun=m_worldSelection==WorldSelection::Sun;
            const auto operation=sun?ImGuizmo::ROTATE:(m_operation==ImGuizmo::SCALE?ImGuizmo::TRANSLATE:m_operation);
            Mat4 matrix=Mat4::Identity();
            float lookDistance=1.0f;
            if(sun) {
                LightComponent light{};light.elevationDegrees=m_document.sun.elevationDegrees;light.azimuthDegrees=m_document.sun.azimuthDegrees;
                matrix=BasisAt(SunAnchor(m_target,m_distance),Object::LightDirection(light));
            } else {
                const Vec3 offset=m_document.camera.target-m_document.camera.position;
                lookDistance=std::max(Length(offset),0.25f);
                const Vec3 facing=Dot(offset,offset)<1e-8f?Vec3{0,0,-1}:Normalize(offset);
                matrix=operation==ImGuizmo::TRANSLATE?Mat4::Translate(m_document.camera.position):BasisAt(m_document.camera.position,facing);
            }
            const float step=operation==ImGuizmo::ROTATE?m_snapRotate:m_snapTranslate;
            float snap[]={step,step,step};
            const bool snapping=m_snap!=io.KeyCtrl;
            auto before=m_snapshot;
            const bool manipulated=ImGuizmo::Manipulate(Data(view),Data(projection),operation,sun||!m_local?ImGuizmo::WORLD:ImGuizmo::LOCAL,Data(matrix),nullptr,snapping?snap:nullptr);
            const bool usingNow=ImGuizmo::IsUsing();
            if(usingNow && !m_gizmoWasUsing) {m_undo.push_back(before);if(m_undo.size()>64)m_undo.pop_front();m_redo.clear();}
            if(manipulated) {
                const Vec3 newPosition=Column(matrix,3);
                if(sun) {
                    const Vec3 direction=Normalize(Column(matrix,2));
                    if(Dot(direction,direction)>0.5f) {
                        m_document.sun.elevationDegrees=std::clamp(Degrees(std::asin(std::clamp(-direction.y,-1.0f,1.0f))),-90.0f,90.0f);
                        if(std::abs(std::cos(Radians(m_document.sun.elevationDegrees)))>0.02f)
                            m_document.sun.azimuthDegrees=Degrees(std::atan2(-direction.x,-direction.z));
                    }
                } else if(std::isfinite(newPosition.x) && std::isfinite(newPosition.y) && std::isfinite(newPosition.z)) {
                    if(operation==ImGuizmo::TRANSLATE) {
                        const Vec3 delta=newPosition-m_document.camera.position;
                        m_document.camera.position=newPosition;m_document.camera.target=m_document.camera.target+delta;
                    } else {
                        const Vec3 facing=Normalize(Column(matrix,2));
                        m_document.camera.position=newPosition;
                        m_document.camera.target=newPosition+facing*lookDistance;
                    }
                }
                m_sceneDirty=true;Synchronize();
            }
            m_gizmoWasUsing=usingNow;
        } else m_gizmoWasUsing=false;
        const float cube=std::round(unit*5.2f),margin=std::round(unit*0.55f);
        const ImVec2 cubePosition{origin.x+size.x-cube-margin,origin.y+margin};
        if(size.x>unit*18 && size.y>unit*14) {
            auto cameraView=view;
            ImGuizmo::ViewManipulate(Data(cameraView),m_distance,cubePosition,{cube,cube},IM_COL32(0,0,0,0));
            const Vec3 backward{cameraView.col[0].z,cameraView.col[1].z,cameraView.col[2].z};
            if(Dot(backward,backward)>0.5f && (ImGuizmo::IsUsingViewManipulate() || std::abs(Dot(Normalize(backward),Normalize(m_eye-m_target))-1.0f)>1e-4f)) {
                const Vec3 direction=Normalize(backward);
                m_pitch=std::clamp(std::asin(std::clamp(direction.y,-1.0f,1.0f)),-1.45f,1.45f);
                if(std::abs(direction.y)<0.999f)m_yaw=std::atan2(direction.x,direction.z);
            }
        }
        const bool overCube=ImGuizmo::IsViewManipulateHovered();
        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !overCube)Pick(origin,size);
        if(hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && (IsSelected() || m_worldSelection!=WorldSelection::None))FocusSelection();
        if(size.x>unit*30) {
            const char* hint=size.x>unit*52?Tr("RMB orbit   RMB+WASD fly   MMB pan   Wheel zoom   F focus   W/E/R tools"):Tr("RMB orbit   MMB pan   Wheel zoom");
            draw->AddText({origin.x+margin,origin.y+size.y-unit*2.2f-margin},IM_COL32(200,208,222,150),hint);
        }
        draw->PopClipRect();
        ViewportOverlay(origin,size);
        if(m_contextMenu){ImGui::OpenPopup("##viewportMenu");m_contextMenu=false;}
        ViewportContextMenu();
    }
    ImGui::End();
}
}
