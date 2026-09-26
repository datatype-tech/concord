// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include <Concord/CCamera.h>
#include <algorithm>
#include <cmath>
#include <limits>

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
}
void Workspace::FocusSelection()
{
    if(!IsSelected())return;
    const auto& object=m_document.objects[m_selection];m_target=object.transform.position;
    m_distance=std::max({object.size.x*object.transform.scale.x,object.size.y*object.transform.scale.y,object.size.z*object.transform.scale.z})*2.5f;
    m_distance=std::clamp(m_distance,2.0f,200.0f);
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
    const auto ray=Normalize(forward+right*x+up*y);float closest=std::numeric_limits<float>::max();m_selection=-1;
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
        if(hit && nearHit<closest) {closest=nearHit;m_selection=i;}
    }
}
void Workspace::Viewport()
{
    ImGuizmo::BeginFrame();
    if(ImGui::Begin("Scene")) {
        const bool labels=ImGui::GetContentRegionAvail().x>740;
        if(Design::Action("Move (W)","move",labels?"Move":nullptr,m_operation==ImGuizmo::TRANSLATE))m_operation=ImGuizmo::TRANSLATE;ImGui::SameLine();
        if(Design::Action("Rotate (E)","rotate",labels?"Rotate":nullptr,m_operation==ImGuizmo::ROTATE))m_operation=ImGuizmo::ROTATE;ImGui::SameLine();
        if(Design::Action("Scale (R)","scale",labels?"Scale":nullptr,m_operation==ImGuizmo::SCALE))m_operation=ImGuizmo::SCALE;ImGui::SameLine();
        ImGui::Checkbox("Local",&m_local);ImGui::SameLine();ImGui::Checkbox("Snap",&m_snap);
        ImGui::SameLine();if(Design::Action("Show grid","grid",nullptr,m_showGrid))m_showGrid=!m_showGrid;
        ImGui::SameLine();if(Design::Action("Focus selected (F)","focus"))FocusSelection();
        ImGui::SameLine();if(Design::Action("Camera views","eye"))ImGui::OpenPopup("Camera views");
        if(ImGui::BeginPopup("Camera views")) {
            if(ImGui::MenuItem("Perspective")){m_yaw=0.68573f;m_pitch=0.399f;m_distance=15.42f;m_target={0,1,0};}
            if(ImGui::MenuItem("Front")){m_yaw=0;m_pitch=0;}
            if(ImGui::MenuItem("Side")){m_yaw=1.5708f;m_pitch=0;}
            if(ImGui::MenuItem("Top")){m_yaw=0;m_pitch=1.56f;}
            ImGui::EndPopup();
        }
        const auto origin=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();size.x=std::max(size.x,32.0f);size.y=std::max(size.y,32.0f);
        auto& ui=*m_game.Toolkit();const auto framebufferScale=ImGui::GetIO().DisplayFramebufferScale;
        ui.SetSceneViewport(static_cast<u32>(size.x*framebufferScale.x),static_cast<u32>(size.y*framebufferScale.y));
        ImGui::Image(ui.SceneTexture(),size);
        const bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
        if(hovered && !ImGuizmo::IsUsing()) {
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {m_yaw-=io.MouseDelta.x*0.007f;m_pitch=std::clamp(m_pitch+io.MouseDelta.y*0.007f,-1.45f,1.45f);}
            if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                auto forward=Normalize(m_target-m_eye),right=Normalize(Cross(forward,{0,1,0})),up=Cross(right,forward);
                m_target=m_target+right*(-io.MouseDelta.x*m_distance*0.0015f)+up*(io.MouseDelta.y*m_distance*0.0015f);
            }
            m_distance=std::clamp(m_distance*std::pow(0.85f,io.MouseWheel),0.5f,400.0f);
            if(!io.WantTextInput) {
                if(ImGui::IsKeyPressed(ImGuiKey_F))FocusSelection();
                if(ImGui::IsKeyPressed(ImGuiKey_W))m_operation=ImGuizmo::TRANSLATE;
                if(ImGui::IsKeyPressed(ImGuiKey_E))m_operation=ImGuizmo::ROTATE;
                if(ImGui::IsKeyPressed(ImGuiKey_R))m_operation=ImGuizmo::SCALE;
                if(ImGui::IsKeyPressed(ImGuiKey_Delete))DeleteSelection();
            }
        }
        UpdateCamera();
        // The renderer uses the actual per-frame texture extent; avoid resize-frame aspect stretching.
        m_camera.Get<CameraComponent>()->aspectOverride=size.x/size.y;
        auto view=Mat4::LookAt(m_eye,m_target,{0,1,0});auto projection=Mat4::Perspective(Radians(60.0f),size.x/size.y,0.1f,1000);
        projection.col[1].y=-projection.col[1].y;
        ImGuizmo::SetOrthographic(false);ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());ImGuizmo::SetRect(origin.x,origin.y,size.x,size.y);
        ImGui::GetWindowDrawList()->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
        if(m_showGrid) {
            auto project=[&](Vec3 point,ImVec2& screen) {
                const auto clip=projection*(view*Vec4{point.x,point.y,point.z,1});if(clip.w<0.1f)return false;
                screen={origin.x+(clip.x/clip.w*0.5f+0.5f)*size.x,origin.y+(0.5f-clip.y/clip.w*0.5f)*size.y};return true;
            };
            for(int i=-20;i<=20;++i) {
                ImVec2 a,b;const auto color=i==0?IM_COL32(108,123,161,95):IM_COL32(117,126,143,35);
                if(project({float(i),0,-20},a)&&project({float(i),0,20},b))ImGui::GetWindowDrawList()->AddLine(a,b,color);
                if(project({-20,0,float(i)},a)&&project({20,0,float(i)},b))ImGui::GetWindowDrawList()->AddLine(a,b,color);
            }
        }
        if(IsSelected()) {
            auto& object=m_document.objects[m_selection];auto matrix=object.transform.ToMatrix();
            float snap[]={m_operation==ImGuizmo::ROTATE?15.0f:0.5f,m_operation==ImGuizmo::ROTATE?15.0f:0.5f,m_operation==ImGuizmo::ROTATE?15.0f:0.5f};
            auto before=m_snapshot;
            const bool manipulated=ImGuizmo::Manipulate(Data(view),Data(projection),m_operation,m_local?ImGuizmo::LOCAL:ImGuizmo::WORLD,Data(matrix),nullptr,m_snap?snap:nullptr);
            const bool usingNow=ImGuizmo::IsUsing();
            if(usingNow && !m_gizmoWasUsing) {m_undo.push_back(before);if(m_undo.size()>64)m_undo.pop_front();m_redo.clear();}
            if(manipulated) {object.transform=Decompose(matrix);m_sceneDirty=true;Synchronize();}
            m_gizmoWasUsing=usingNow;
            ImVec2 corners[8];bool valid=true;
            for(int i=0;i<8;++i) {
                Vec4 corner{(i&1?0.5f:-0.5f)*object.size.x,(i&2?0.5f:-0.5f)*object.size.y,(i&4?0.5f:-0.5f)*object.size.z,1};
                auto clip=projection*(view*(matrix*corner));if(clip.w<=0.01f){valid=false;break;}
                corners[i]={origin.x+(clip.x/clip.w*0.5f+0.5f)*size.x,origin.y+(0.5f-clip.y/clip.w*0.5f)*size.y};
            }
            if(valid)for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))ImGui::GetWindowDrawList()->AddLine(corners[i],corners[i|bit],IM_COL32(73,235,212,210),1.5f);
        } else m_gizmoWasUsing=false;
        if(hovered && ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())Pick(origin,size);
        ImGui::GetWindowDrawList()->AddText({origin.x+12,origin.y+size.y-ImGui::GetFontSize()-10},IM_COL32(162,175,196,215),size.x>850?"RMB orbit   MMB pan   Scroll zoom   F focus   W/E/R tools":"RMB orbit   Scroll zoom   F focus");
        ImGui::GetWindowDrawList()->PopClipRect();
    }
    ImGui::End();
}
}
