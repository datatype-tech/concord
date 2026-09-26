// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cstdio>
#include <regex>
#include <sstream>

namespace Concord::Editor {
void Workspace::Hierarchy()
{
    if(ImGui::Begin("Hierarchy")) {
        if(Design::Action("Add object","plus","Add"))ImGui::OpenPopup("Add object");ImGui::SameLine();
        ImGui::BeginDisabled(!IsSelected());if(Design::Action("Duplicate selected","copy"))DuplicateSelection();ImGui::SameLine();
        if(Design::Action("Focus selected","focus"))FocusSelection();ImGui::EndDisabled();
        if(ImGui::BeginPopup("Add object")) {
            if(ImGui::MenuItem("Cube"))AddBox();
            if(ImGui::MenuItem("Ground platform")) {AddBox();auto& object=m_document.objects.back();object.kind=SceneObjectKind::StaticBox;object.name="Ground";object.size={10,0.25f,10};object.transform.position={0,-0.125f,0};Synchronize();}
            if(ImGui::MenuItem("Wall")) {AddBox();auto& object=m_document.objects.back();object.name="Wall";object.size={5,3,0.3f};object.transform.position={0,1.5f,0};Synchronize();}
            ImGui::EndPopup();
        }
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##filterObjects","Filter objects...",m_hierarchyFilter,sizeof(m_hierarchyFilter));
        ImGui::Separator();
        for(int i=0;i<static_cast<int>(m_document.objects.size());++i) {
            auto& object=m_document.objects[i];
            if(m_hierarchyFilter[0] && object.name.find(m_hierarchyFilter)==std::string::npos)continue;
            ImGui::PushID(i);
            Design::Image("cube",ImGui::GetFontSize(),object.visible?IM_COL32(155,166,212,255):IM_COL32(75,81,99,255));ImGui::SameLine();
            if(ImGui::Selectable(object.name.c_str(),m_selection==i))m_selection=i;
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))FocusSelection();
            if(ImGui::BeginPopupContextItem()) {
                m_selection=i;
                if(ImGui::MenuItem("Focus"))FocusSelection();
                if(ImGui::MenuItem(object.visible?"Hide object":"Show object")){Checkpoint();object.visible=!object.visible;Synchronize();}
                if(ImGui::MenuItem("Duplicate"))DuplicateSelection();
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::Spacing();Design::Eyebrow("ENVIRONMENT");ImGui::TextColored(Design::Muted,"Camera  /  Sunlight");
    }
    ImGui::End();
}
void Workspace::Inspector()
{
    if(ImGui::Begin("Inspector")) {
        if(ImGui::BeginTabBar("Inspector tabs")) {
        if(ImGui::BeginTabItem("Object")) {
        if(IsSelected()) {
            auto& object=m_document.objects[m_selection];const auto before=object;bool changed=false;
            char name[256]{};std::snprintf(name,sizeof(name),"%s",object.name.c_str());
            ImGui::SetNextItemWidth(-1);
            bool edit=ImGui::InputText("##objectName",name,sizeof(name));RecordEdit();if(edit){object.name=name;changed=true;}
            bool visible=object.visible;
            if(ImGui::Checkbox("Visible in game",&visible)){Checkpoint();object.visible=visible;changed=true;}
            bool shadow=object.castShadow;
            if(ImGui::Checkbox("Cast shadows",&shadow)){Checkpoint();object.castShadow=shadow;changed=true;}
            const char* kinds[]={"Box","Thin plane","Static physics box","Dynamic physics box"};
            int kind=static_cast<int>(object.kind);
            if(ImGui::Combo("Body",&kind,kinds,4)){Checkpoint();object.kind=static_cast<SceneObjectKind>(kind);changed=true;}
            if(object.kind==SceneObjectKind::DynamicBox || object.kind==SceneObjectKind::StaticBox) {
                if(object.kind==SceneObjectKind::DynamicBox) {
                    changed|=ImGui::DragFloat("Mass",&object.mass,0.1f,0.001f,100000,"%.2f",ImGuiSliderFlags_AlwaysClamp);RecordEdit();
                    bool lock=object.lockRotation;if(ImGui::Checkbox("Lock rotation",&lock)){Checkpoint();object.lockRotation=lock;changed=true;}
                }
                changed|=ImGui::SliderFloat("Friction",&object.friction,0,1);RecordEdit();
                changed|=ImGui::SliderFloat("Bounce",&object.restitution,0,1);RecordEdit();
                ImGui::TextWrapped("Physics simulates when you press Play.");
            }
            ImGui::Spacing();ImGui::SeparatorText("Transform");
            ImGui::PushItemWidth(-1);
            ImGui::TextDisabled("Position");edit=ImGui::DragFloat3("##position",&object.transform.position.x,0.05f,-100000,100000);RecordEdit();changed|=edit;
            ImGui::TextDisabled("Rotation (degrees)");edit=ImGui::DragFloat3("##rotation",&object.transform.rotation.x,0.5f,-36000,36000);RecordEdit();changed|=edit;
            ImGui::TextDisabled("Scale");edit=ImGui::DragFloat3("##scale",&object.transform.scale.x,0.01f,0.001f,1000,"%.3f",ImGuiSliderFlags_AlwaysClamp);RecordEdit();changed|=edit;
            ImGui::TextDisabled("Mesh dimensions");edit=ImGui::DragFloat3("##size",&object.size.x,0.05f,0.001f,1000,"%.3f",ImGuiSliderFlags_AlwaysClamp);RecordEdit();changed|=edit;
            ImGui::SeparatorText("Surface");
            float color[]={ColorR(object.material.albedo)/255.0f,ColorG(object.material.albedo)/255.0f,ColorB(object.material.albedo)/255.0f};
            ImGui::TextDisabled("Base color");edit=ImGui::ColorEdit3("##Albedo",color,ImGuiColorEditFlags_NoInputs);RecordEdit();
            if(edit) {object.material.albedo=MakeColor(static_cast<u8>(color[0]*255),static_cast<u8>(color[1]*255),static_cast<u8>(color[2]*255));changed=true;}
            ImGui::TextDisabled("Metallic");edit=ImGui::SliderFloat("##metallic",&object.material.metallic,0,1);RecordEdit();changed|=edit;
            ImGui::TextDisabled("Roughness");edit=ImGui::SliderFloat("##roughness",&object.material.roughness,0,1);RecordEdit();changed|=edit;
            ImGui::TextDisabled("Emission");edit=ImGui::DragFloat("##emission",&object.material.emissive,0.05f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);RecordEdit();changed|=edit;
            ImGui::PopItemWidth();
            if(changed) {
                try{m_document.Validate();m_sceneDirty=true;Synchronize();}
                catch(const std::exception& error){object=before;Report(error);}
            }
            ImGui::Spacing();if(Design::Action("Focus object","focus","Focus"))FocusSelection();
            ImGui::SameLine();if(Design::Action("Delete object","delete"))DeleteSelection();
        } else ImGui::TextDisabled("Select an object to inspect it.");
        ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("World")){SceneSettings();ImGui::EndTabItem();}
        ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
void Workspace::ProjectDialog()
{
    ImGui::SetNextWindowSize({std::min(760.0f,ImGui::GetMainViewport()->Size.x-60),0},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("New project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        Design::Heading("Create your next world");
        ImGui::TextWrapped("Choose a location and a name. The editor creates and configures the project for you.");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-200);ImGui::InputText("Location",m_projectInput,sizeof(m_projectInput));ImGui::SameLine();
        if(ImGui::Button("Browse...")) {auto path=ChooseFolder(L"Choose parent folder for your project");if(!path.empty())std::snprintf(m_projectInput,sizeof(m_projectInput),"%s",Utf8Text(path).c_str());}
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-150);ImGui::InputText("Project name",m_projectName,sizeof(m_projectName));
        ImGui::Spacing();ImGui::SeparatorText("Template");
        ImGui::RadioButton("3D starter scene",&m_newTemplate,0);ImGui::SameLine();ImGui::RadioButton("Empty scene",&m_newTemplate,1);
        ImGui::TextWrapped(m_newTemplate==0?"Start with a lit stage and three material samples.":"An empty scene with a camera and sunlight, ready for your ideas.");
        if(ImGui::CollapsingHeader("Advanced toolchain settings")) {
            ImGui::SetNextItemWidth(550);ImGui::InputText("CLI",m_cli,sizeof(m_cli));ImGui::SameLine();
            if(ImGui::Button("Choose CLI")) {auto path=ChooseExecutable();if(!path.empty())std::snprintf(m_cli,sizeof(m_cli),"%s",Utf8Text(path).c_str());}
            ImGui::SetNextItemWidth(550);ImGui::InputText("SDK",m_sdk,sizeof(m_sdk));ImGui::SameLine();
            if(ImGui::Button("Choose SDK")) {auto path=ChooseFolder(L"Choose Concord SDK");if(!path.empty())std::snprintf(m_sdk,sizeof(m_sdk),"%s",Utf8Text(path).c_str());}
            ImGui::TextWrapped("Leave SDK empty to let the CLI download the published GitHub release.");
        }
        ImGui::BeginDisabled(m_process.Busy());
        if(Design::Action("Create project","plus","Create project",false,true))Attempt([&] {
            if(!m_projectInput[0])throw std::runtime_error("Enter a new project path");
            const std::string name=m_projectName;
            if(name.empty() || name=="." || name==".." || name.find_first_of("/\\:*?\"<>|")!=std::string::npos)throw std::runtime_error("Enter a valid project folder name");
            auto project=std::filesystem::absolute(Utf8Path(m_projectInput)/Utf8Path(name));
            if(std::filesystem::exists(project))throw std::runtime_error("New project path must not already exist");
            Save();std::vector<std::wstring> arguments={L"init",project.wstring()};
            if(m_sdk[0]) {arguments.push_back(L"--sdk");arguments.push_back(Utf8Path(m_sdk).wstring());}
            m_process.Start(Utf8Path(m_cli),arguments,m_project.empty()?std::filesystem::current_path():m_project);
            m_pendingCreate=Utf8Text(project);m_wasBusy=true;ImGui::CloseCurrentPopup();
        });
        ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
        ImGui::TextWrapped("%s",m_status.c_str());ImGui::EndPopup();
    }
}
}
