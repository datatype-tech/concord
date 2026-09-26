// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/Editor.h"
#include "editor/Design.h"
#include "editor/Localization.h"
#include "editor/PropertyGrid.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace Concord::Editor {
namespace {
const char* KindIcon(SceneObjectKind kind) {return kind==SceneObjectKind::Plane?"grid":"cube";}
ImU32 KindColor(SceneObjectKind kind,bool visible)
{
    if(!visible)return IM_COL32(92,98,110,255);
    switch(kind) {
    case SceneObjectKind::Plane: return IM_COL32(140,196,168,255);
    case SceneObjectKind::StaticBox: return IM_COL32(118,176,236,255);
    case SceneObjectKind::DynamicBox: return IM_COL32(240,166,92,255);
    default: return IM_COL32(176,184,206,255);
    }
}
const char* KindName(SceneObjectKind kind)
{
    const char* names[]={"Box","Thin plane","Static physics box","Dynamic physics box"};
    return Tr(names[static_cast<int>(kind)]);
}
bool ContainsFolded(const std::string& text,const char* query)
{
    if(!query[0])return true;
    std::string a=text,b=query;
    for(auto* s:{&a,&b})for(char& c:*s)if(static_cast<unsigned char>(c)<128)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return a.find(b)!=std::string::npos;
}
bool EnvironmentRow(const char* icon,const char* label,ImU32 tint)
{
    const float unit=ImGui::GetFontSize();
    const auto position=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::Selectable((std::string("##")+label).c_str(),false,ImGuiSelectableFlags_None,{0,unit*1.35f});
    Design::Icon(icon,{position.x+unit*0.2f,position.y+unit*0.18f},unit,tint);
    ImGui::GetWindowDrawList()->AddText({position.x+unit*1.6f,position.y+unit*0.18f},ImGui::GetColorU32(ImGuiCol_Text),label);
    return clicked;
}
}

void Workspace::AddObjectMenu()
{
    if(ImGui::MenuItem(Tr("Cube")))AddObject(ObjectPreset::Cube);
    if(ImGui::MenuItem(Tr("Plane")))AddObject(ObjectPreset::Plane);
    ImGui::Separator();
    ImGui::TextDisabled("%s",Tr("Physics"));
    if(ImGui::MenuItem(Tr("Ground")))AddObject(ObjectPreset::Ground);
    if(ImGui::MenuItem(Tr("Wall")))AddObject(ObjectPreset::Wall);
    if(ImGui::MenuItem(Tr("Static physics box")))AddObject(ObjectPreset::StaticBody);
    if(ImGui::MenuItem(Tr("Dynamic physics box")))AddObject(ObjectPreset::DynamicBody);
}

void Workspace::Hierarchy()
{
    if(ImGui::Begin(TrId("Hierarchy").c_str())) {
        const float unit=ImGui::GetFontSize(),frame=ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-frame-ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##filterObjects",Tr("Search objects..."),m_hierarchyFilter,sizeof(m_hierarchyFilter));
        ImGui::SameLine();
        if(Design::Action("Add object","plus"))ImGui::OpenPopup("##addObject");
        if(ImGui::BeginPopup("##addObject")){AddObjectMenu();ImGui::EndPopup();}
        ImGui::Spacing();
        Design::Eyebrow(Tr("ENVIRONMENT"));
        if(EnvironmentRow("eye",Tr("Game camera"),IM_COL32(236,208,120,255)))m_selectWorldTab=true;
        if(EnvironmentRow("focus",Tr("Sun light"),IM_COL32(250,196,90,255)))m_selectWorldTab=true;
        ImGui::Spacing();
        Design::Eyebrow(Tr("SCENE OBJECTS"));
        const auto count=std::to_string(m_document.objects.size());
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-ImGui::CalcTextSize(count.c_str()).x));
        ImGui::TextColored(Design::Muted,"%s",count.c_str());
        if(ImGui::BeginChild("##objects",{0,0},ImGuiChildFlags_None)) {
            int moveFrom=-1,moveTo=-1,toggle=-1,remove=-1;
            const float row=std::round(unit*1.45f);
            for(int i=0;i<static_cast<int>(m_document.objects.size());++i) {
                auto& object=m_document.objects[i];
                if(!ContainsFolded(object.name,m_hierarchyFilter))continue;
                ImGui::PushID(i);
                const auto position=ImGui::GetCursorScreenPos();
                const float width=ImGui::GetContentRegionAvail().x;
                if(m_renaming==i) {
                    Design::Icon(KindIcon(object.kind),{position.x+unit*0.2f,position.y+(row-unit)*0.5f},unit,KindColor(object.kind,object.visible));
                    ImGui::SetCursorScreenPos({position.x+unit*1.5f,position.y+(row-frame)*0.5f});
                    ImGui::SetNextItemWidth(width-unit*1.5f);
                    if(m_focusRename){ImGui::SetKeyboardFocusHere();m_focusRename=false;}
                    const bool commit=ImGui::InputText("##rename",m_renameBuffer,sizeof(m_renameBuffer),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll);
                    const bool cancel=ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
                    if(cancel)m_renaming=-1;
                    else if(commit || ImGui::IsItemDeactivated()) {
                        std::string name=m_renameBuffer;
                        if(!name.empty() && name!=object.name && name.find_first_of("\r\n")==std::string::npos) {
                            Checkpoint();object.name=name.substr(0,255);
                            try{m_document.Validate();Synchronize();}catch(const std::exception& error){Undo();Report(error);}
                        }
                        m_renaming=-1;
                    }
                    ImGui::SetCursorScreenPos({position.x,position.y+row});ImGui::Dummy({0,0});
                    ImGui::PopID();continue;
                }
                ImGui::SetNextItemAllowOverlap();
                if(ImGui::Selectable("##row",m_selection==i,ImGuiSelectableFlags_AllowDoubleClick,{0,row})) {
                    m_selection=i;
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))BeginRename(i);
                }
                if(ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("CONCORD_OBJECT",&i,sizeof(i));
                    ImGui::TextUnformatted(object.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if(ImGui::BeginDragDropTarget()) {
                    if(const auto* payload=ImGui::AcceptDragDropPayload("CONCORD_OBJECT")){moveFrom=*static_cast<const int*>(payload->Data);moveTo=i;}
                    ImGui::EndDragDropTarget();
                }
                const bool hovered=ImGui::IsItemHovered();
                if(ImGui::BeginPopupContextItem("##objectMenu")) {
                    m_selection=i;
                    if(ImGui::MenuItem(Tr("Rename"),"F2"))BeginRename(i);
                    if(ImGui::MenuItem(Tr("Duplicate"),"Ctrl+D"))DuplicateSelection();
                    if(ImGui::MenuItem(Tr("Copy"),"Ctrl+C"))CopySelection();
                    if(ImGui::MenuItem(Tr("Paste"),"Ctrl+V",false,m_copiedObject.has_value()))PasteObject();
                    ImGui::Separator();
                    if(ImGui::MenuItem(object.visible?Tr("Hide"):Tr("Show")))toggle=i;
                    if(ImGui::MenuItem(Tr("Focus"),"F"))FocusSelection();
                    ImGui::Separator();
                    if(ImGui::MenuItem(Tr("Delete"),"Del"))remove=i;
                    ImGui::EndPopup();
                }
                auto* draw=ImGui::GetWindowDrawList();
                Design::Icon(KindIcon(object.kind),{position.x+unit*0.2f,position.y+(row-unit)*0.5f},unit,KindColor(object.kind,object.visible));
                draw->PushClipRect(position,{position.x+width-frame,position.y+row},true);
                draw->AddText({position.x+unit*1.5f,position.y+(row-unit)*0.5f},
                    ImGui::GetColorU32(object.visible?ImGuiCol_Text:ImGuiCol_TextDisabled),object.name.c_str());
                draw->PopClipRect();
                if(hovered || !object.visible || ImGui::IsPopupOpen("##objectMenu")) {
                    ImGui::SetCursorScreenPos({position.x+width-frame,position.y+(row-frame)*0.5f});
                    if(Design::Ghost(object.visible?"Hide object":"Show object","eye",nullptr,!object.visible,frame))toggle=i;
                }
                ImGui::SetCursorScreenPos({position.x,position.y+row});ImGui::Dummy({0,0});
                ImGui::PopID();
            }
            if(toggle>=0){Checkpoint();m_document.objects[toggle].visible=!m_document.objects[toggle].visible;Synchronize();}
            if(remove>=0){m_selection=remove;DeleteSelection();}
            if(moveFrom>=0 && moveTo>=0 && moveFrom!=moveTo) {
                Checkpoint();auto moved=m_document.objects[moveFrom];
                m_document.objects.erase(m_document.objects.begin()+moveFrom);
                m_document.objects.insert(m_document.objects.begin()+moveTo,moved);
                m_selection=moveTo;Synchronize();
            }
            if(m_document.objects.empty()) {
                ImGui::Spacing();ImGui::TextColored(Design::Muted,"%s",Tr("This scene is empty."));
                if(Design::Action("##addFirst","plus",Tr("Add object")))ImGui::OpenPopup("##addFirstObject");
                if(ImGui::BeginPopup("##addFirstObject")){AddObjectMenu();ImGui::EndPopup();}
            }
            if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())m_selection=-1;
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void Workspace::Inspector()
{
    if(ImGui::Begin(TrId("Inspector").c_str())) {
        const bool selectObject=m_selection!=m_lastInspected && IsSelected();
        m_lastInspected=m_selection;
        if(ImGui::BeginTabBar("##inspectorTabs")) {
            if(ImGui::BeginTabItem(TrId("Object").c_str(),nullptr,selectObject && !m_selectWorldTab?ImGuiTabItemFlags_SetSelected:0)) {
                ObjectInspector();ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem(TrId("World").c_str(),nullptr,m_selectWorldTab?ImGuiTabItemFlags_SetSelected:0)) {
                SceneSettings();ImGui::EndTabItem();
            }
            m_selectWorldTab=false;
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void Workspace::ObjectInspector()
{
    const float unit=ImGui::GetFontSize();
    if(!IsSelected()) {
        ImGui::Dummy({0,unit});
        const float icon=unit*2.2f;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x-icon)*0.5f);
        Design::Image("focus",icon,ImGui::GetColorU32(Design::Muted));
        const char* title=Tr("Nothing selected");const char* body=Tr("Select an object in the hierarchy or viewport.");
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(title).x)*0.5f);ImGui::TextUnformatted(title);
        ImGui::PushTextWrapPos();ImGui::TextColored(Design::Muted,"%s",body);ImGui::PopTextWrapPos();
        return;
    }
    auto& object=m_document.objects[m_selection];const auto before=object;bool changed=false;
    char name[256]{};std::snprintf(name,sizeof(name),"%s",object.name.c_str());
    Design::Image(KindIcon(object.kind),unit*1.2f,KindColor(object.kind,object.visible));ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(ImGui::InputText("##objectName",name,sizeof(name)) && name[0]){object.name=name;changed=true;}
    RecordEdit();
    Design::Badge(KindName(object.kind),ImGui::ColorConvertU32ToFloat4(KindColor(object.kind,true)));
    ImGui::Spacing();
    if(ImGui::CollapsingHeader(TrId("Transform").c_str(),ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##transform")) {
        changed|=PropertyGrid::Vector("Position",object.transform.position,0.05f,-100000,100000);RecordEdit();
        changed|=PropertyGrid::Vector("Rotation",object.transform.rotation,0.5f,-36000,36000,"%.1f");RecordEdit();
        changed|=PropertyGrid::Vector("Scale",object.transform.scale,0.01f,0.001f,1000,"%.3f");RecordEdit();
        PropertyGrid::End();
        if(Design::Ghost("Reset transform","rotate",Tr("Reset"))){Checkpoint();object.transform={};changed=true;}
    }
    if(ImGui::CollapsingHeader(TrId("Shape").c_str(),ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##shape")) {
        const char* kinds[]={"Box","Thin plane","Static physics box","Dynamic physics box"};
        int kind=static_cast<int>(object.kind);
        if(PropertyGrid::Combo("Type",kind,kinds,4)){Checkpoint();object.kind=static_cast<SceneObjectKind>(kind);changed=true;}
        changed|=PropertyGrid::Vector("Dimensions",object.size,0.05f,0.001f,1000,"%.3f");RecordEdit();
        PropertyGrid::End();
    }
    if(ImGui::CollapsingHeader(TrId("Material").c_str(),ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##material")) {
        changed|=PropertyGrid::Color("Base color",object.material.albedo);RecordEdit();
        PropertyGrid::Row("Presets");
        const ColorRGBA presets[]={COLOR_RGB(62,190,181),COLOR_RGB(92,143,230),COLOR_RGB(146,115,238),COLOR_RGB(236,98,110),
                                   COLOR_RGB(242,162,75),COLOR_RGB(236,214,120),COLOR_RGB(222,226,232),COLOR_RGB(52,58,70)};
        const float swatch=ImGui::GetFrameHeight()*0.8f;
        for(int index=0;index<8;++index) {
            if(index)ImGui::SameLine(0,ImGui::GetStyle().ItemInnerSpacing.x);
            if(ImGui::GetContentRegionAvail().x<swatch)ImGui::NewLine();
            ImGui::PushID(index);
            const ImVec4 color{ColorR(presets[index])/255.0f,ColorG(presets[index])/255.0f,ColorB(presets[index])/255.0f,1};
            if(ImGui::ColorButton("##preset",color,ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoBorder,{swatch,swatch})) {
                Checkpoint();object.material.albedo=presets[index];changed=true;
            }
            ImGui::PopID();
        }
        changed|=PropertyGrid::Slider("Metallic",object.material.metallic,0,1);RecordEdit();
        changed|=PropertyGrid::Slider("Roughness",object.material.roughness,0,1);RecordEdit();
        changed|=PropertyGrid::Float("Emission",object.material.emissive,0.05f,0,100,"%.2f");RecordEdit();
        PropertyGrid::End();
    }
    if(ImGui::CollapsingHeader(TrId("Rendering").c_str(),ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##rendering")) {
        bool visible=object.visible;
        if(PropertyGrid::Check("Visible in game",visible)){Checkpoint();object.visible=visible;changed=true;}
        bool shadow=object.castShadow;
        if(PropertyGrid::Check("Cast shadows",shadow)){Checkpoint();object.castShadow=shadow;changed=true;}
        PropertyGrid::End();
    }
    if(object.kind==SceneObjectKind::DynamicBox || object.kind==SceneObjectKind::StaticBox) {
        if(ImGui::CollapsingHeader(TrId("Physics").c_str(),ImGuiTreeNodeFlags_DefaultOpen) && PropertyGrid::Begin("##physics")) {
            if(object.kind==SceneObjectKind::DynamicBox) {
                changed|=PropertyGrid::Float("Mass (kg)",object.mass,0.1f,0.001f,100000,"%.2f");RecordEdit();
                bool lock=object.lockRotation;
                if(PropertyGrid::Check("Lock rotation",lock)){Checkpoint();object.lockRotation=lock;changed=true;}
            }
            changed|=PropertyGrid::Slider("Friction",object.friction,0,1);RecordEdit();
            changed|=PropertyGrid::Slider("Bounciness",object.restitution,0,1);RecordEdit();
            PropertyGrid::End();
            ImGui::PushTextWrapPos();ImGui::TextColored(Design::Muted,"%s",Tr("Physics runs in the game started by Play."));ImGui::PopTextWrapPos();
        }
    }
    if(changed) {
        try{m_document.Validate();m_sceneDirty=true;Synchronize();}
        catch(const std::exception& error){object=before;Report(error);}
    }
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
    if(Design::Action("##focusObject","focus",Tr("Focus")))FocusSelection();
    ImGui::SameLine();if(Design::Action("##duplicateObject","copy",Tr("Duplicate")))DuplicateSelection();
    ImGui::SameLine();if(Design::Action("Delete object","delete"))DeleteSelection();
}

void Workspace::ProjectDialog()
{
    if(!Design::BeginDialog(TrId("New project","NewProject").c_str(),38))return;
    const float unit=ImGui::GetFontSize();
    if(ImGui::IsWindowAppearing())m_createError.clear();
    Design::Heading(Tr("Create a new project"));
    ImGui::TextColored(Design::Muted,"%s",Tr("Choose a template, a name and a location. The editor sets everything up for you."));
    ImGui::Spacing();
    const float spacing=ImGui::GetStyle().ItemSpacing.x,cardWidth=(ImGui::GetContentRegionAvail().x-spacing)*0.5f,cardHeight=unit*4.6f;
    const struct {const char* icon;const char* title;const char* body;} templates[]={
        {"cube","3D starter scene","A lit stage with a physics floor and three material samples."},
        {"file","Empty scene","Only a camera and sunlight, ready for your own ideas."}};
    auto* draw=ImGui::GetWindowDrawList();
    for(int index=0;index<2;++index) {
        if(index)ImGui::SameLine();
        ImGui::PushID(index);
        const auto position=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("##template",{cardWidth,cardHeight}))m_newTemplate=index;
        const bool selected=m_newTemplate==index;
        const float hover=Design::Fade(ImGui::GetItemID(),ImGui::IsItemHovered());
        const float rounding=ImGui::GetStyle().FrameRounding+2;
        draw->AddRectFilled(position,{position.x+cardWidth,position.y+cardHeight},
            selected?ImGui::GetColorU32({Design::Accent.x,Design::Accent.y,Design::Accent.z,0.14f}):Design::Blend(IM_COL32(255,255,255,8),IM_COL32(255,255,255,16),hover),rounding);
        draw->AddRect(position,{position.x+cardWidth,position.y+cardHeight},
            selected?ImGui::GetColorU32(Design::Accent):ImGui::GetColorU32(ImGuiCol_Border),rounding,0,selected?2.0f:1.0f);
        Design::Icon(templates[index].icon,{position.x+unit*0.8f,position.y+unit*0.8f},unit*1.3f,selected?ImGui::GetColorU32(Design::Accent):ImGui::GetColorU32(ImGuiCol_Text));
        draw->AddText({position.x+unit*2.6f,position.y+unit*0.95f},ImGui::GetColorU32(ImGuiCol_Text),Tr(templates[index].title));
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),{position.x+unit*0.8f,position.y+unit*2.4f},ImGui::GetColorU32(ImGuiCol_TextDisabled),
            Tr(templates[index].body),nullptr,cardWidth-unit*1.6f);
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted(Tr("Project name"));
    ImGui::SetNextItemWidth(-FLT_MIN);ImGui::InputText("##projectName",m_projectName,sizeof(m_projectName));
    ImGui::TextUnformatted(Tr("Location"));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Design::ButtonWidth(Tr("Browse..."))-spacing);
    ImGui::InputText("##projectLocation",m_projectInput,sizeof(m_projectInput));
    ImGui::SameLine();
    if(Design::Action("##browseLocation","folder",Tr("Browse..."))) {
        auto path=ChooseFolder(WideText(Tr("Choose the parent folder for your project")).c_str());
        if(!path.empty())std::snprintf(m_projectInput,sizeof(m_projectInput),"%s",Utf8Text(path).c_str());
    }
    const auto preview=Utf8Text(Utf8Path(m_projectInput)/Utf8Path(m_projectName));
    ImGui::PushTextWrapPos();ImGui::TextColored(Design::Muted,"%s  %s",Tr("Creates"),preview.c_str());ImGui::PopTextWrapPos();
    if(ImGui::CollapsingHeader(TrId("Advanced toolchain settings").c_str())) {
        ImGui::TextUnformatted(Tr("Concord CLI"));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Design::ButtonWidth(Tr("Browse..."))-spacing);ImGui::InputText("##cli",m_cli,sizeof(m_cli));
        ImGui::SameLine();if(Design::Action("##chooseCli","folder",Tr("Browse..."))){auto path=ChooseExecutable();if(!path.empty())std::snprintf(m_cli,sizeof(m_cli),"%s",Utf8Text(path).c_str());}
        ImGui::TextUnformatted(Tr("Engine SDK"));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-Design::ButtonWidth(Tr("Browse..."))-spacing);ImGui::InputText("##sdk",m_sdk,sizeof(m_sdk));
        ImGui::SameLine();if(Design::Action("##chooseSdk","folder",Tr("Browse..."))){auto path=ChooseFolder(WideText(Tr("Choose the Concord SDK folder")).c_str());if(!path.empty())std::snprintf(m_sdk,sizeof(m_sdk),"%s",Utf8Text(path).c_str());}
        ImGui::TextColored(Design::Muted,"%s",Tr("Leave empty to let the CLI download the published release."));
    }
    if(!m_createError.empty()){ImGui::PushTextWrapPos();ImGui::TextColored({0.96f,0.55f,0.45f,1},"%s",m_createError.c_str());ImGui::PopTextWrapPos();}
    ImGui::Spacing();
    const float buttons=Design::ButtonWidth(Tr("Cancel"),false)+Design::ButtonWidth(Tr("Create project"))+spacing;
    Design::AlignRight(buttons);
    if(Design::Action("##cancelProject","",Tr("Cancel")))ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_process.Busy());
    if(Design::Action("##createProject","plus",Tr("Create project"),false,true)) {
        try {
            if(!m_projectInput[0])throw std::runtime_error(Tr("Choose a location for the new project."));
            const std::string name=m_projectName;
            if(name.empty() || name=="." || name==".." || name.find_first_of("/\\:*?\"<>|")!=std::string::npos)throw std::runtime_error(Tr("Enter a valid project folder name."));
            auto project=std::filesystem::absolute(Utf8Path(m_projectInput)/Utf8Path(name));
            if(std::filesystem::exists(project))throw std::runtime_error(Tr("A folder with this name already exists in that location."));
            if(!m_cli[0] || !std::filesystem::is_regular_file(Utf8Path(m_cli)))throw std::runtime_error(Tr("Concord CLI (concord.exe) was not found. Choose it under Advanced toolchain settings."));
            Save();std::vector<std::wstring> arguments={L"init",project.wstring()};
            if(m_sdk[0]) {arguments.push_back(L"--sdk");arguments.push_back(Utf8Path(m_sdk).wstring());}
            m_process.Start(Utf8Path(m_cli),arguments,m_project.empty()?std::filesystem::current_path():m_project);
            m_pendingCreate=Utf8Text(project);m_wasBusy=true;ImGui::CloseCurrentPopup();
            Attempt([&]{SavePreferences();});
        } catch(const std::exception& error) {m_createError=error.what();}
    }
    ImGui::EndDisabled();
    ImGui::EndPopup();
}
}
