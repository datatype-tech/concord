// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/WorkspaceLayouts.h"
#include "editor/SceneDocument.h"

#include <hello_imgui/docking_params.h>
#include <imgui_internal.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>
#include <stdexcept>

/*
 * Named split application below is adapted from Hello ImGui's
 * src/hello_imgui/internal/docking_details.cpp: DoSplit,
 * ApplyDockingSplits and ApplyWindowDockingLocations, revision
 * 3135ade34abcbcfea0bf0dd8c8c4e84a1a350415.
 * https://github.com/pthom/hello_imgui
 * The original DockingParams, DockingSplit and DockableWindow declarations
 * are used directly. Concord owns the native window, event loop and Vulkan backend.
 *
 * The MIT License (MIT)
 * Copyright (c) 2019-2020 Pascal Thomet
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
namespace Concord::Editor {
namespace {
constexpr std::array<const char*,6> LayoutNames{"SceneWide","SceneCompact","CodeWide","CodeCompact","UIWide","UICompact"};

/** ImGui's ini reader assumes a valid acyclic docking tree; reject damaged layout files before loading them. */
bool ValidDockingIni(const std::string& ini,ImGuiID expectedRoot)
{
    struct Node {int depth=0,children=0;};
    std::map<ImGuiID,Node> nodes;
    bool docking=false,rootFound=false;std::istringstream input(ini);std::string line;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.starts_with("[")) {docking=line=="[Docking][Data]";continue;}
        if(!docking)continue;
        const auto start=line.find_first_not_of(" \t");if(start==std::string::npos)continue;
        const char* cursor=line.c_str()+start;
        if(line.compare(start,9,"DockSpace")==0)cursor+=9;
        else if(line.compare(start,8,"DockNode")==0)cursor+=8;
        else return false;
        unsigned int id=0,parent=0,window=0;int used=0,x=0,y=0;
        if(std::sscanf(cursor," ID=0x%X%n",&id,&used)!=1 || id==0 || nodes.contains(id) || nodes.size()>=256)return false;
        cursor+=used;
        if(std::sscanf(cursor," Parent=0x%X%n",&parent,&used)==1) {cursor+=used;if(!nodes.contains(parent))return false;}
        if(std::sscanf(cursor," Window=0x%X%n",&window,&used)==1)cursor+=used;
        if(parent==0) {
            if(std::sscanf(cursor," Pos=%d,%d%n",&x,&y,&used)!=2)return false;
            cursor+=used;
            if(std::sscanf(cursor," Size=%d,%d%n",&x,&y,&used)!=2)return false;
        } else {
            if(std::sscanf(cursor," SizeRef=%d,%d%n",&x,&y,&used)!=2)return false;
            if(++nodes[parent].children>2 || nodes[parent].depth>=32)return false;
        }
        if(x<=0 || y<=0 || x>32767 || y>32767)return false;
        nodes.emplace(id,Node{parent?nodes[parent].depth+1:0,0});
        if(id==expectedRoot && parent==0)rootFound=true;
    }
    return rootFound;
}

int LayoutIndex(int page,bool compact)
{
    if(page<1 || page>3)throw std::runtime_error("Unknown editor workspace");
    return (page-1)*2+(compact?1:0);
}

HelloImGui::DockingParams DefaultLayout(int page,bool compact,bool output)
{
    HelloImGui::DockingParams layout;
    layout.layoutName=LayoutNames[LayoutIndex(page,compact)];
    layout.mainDockSpaceNodeFlags=ImGuiDockNodeFlags_None;
    layout.dockingSplits.emplace_back("MainDockSpace","ProjectSpace",ImGuiDir_Left,compact?0.29f:0.215f);
    if(page==1) {
        if(!compact)layout.dockingSplits.emplace_back("MainDockSpace","InspectorSpace",ImGuiDir_Right,0.27f);
        layout.dockingSplits.emplace_back("ProjectSpace","HierarchySpace",ImGuiDir_Up,0.58f);
        layout.dockableWindows.emplace_back("###Hierarchy","HierarchySpace");
        layout.dockableWindows.emplace_back("###Inspector",compact?"HierarchySpace":"InspectorSpace");
        layout.dockableWindows.emplace_back("###Scene","MainDockSpace");
    } else if(page==2)layout.dockableWindows.emplace_back("###ConcordScript","MainDockSpace");
    else {
        if(!compact)layout.dockingSplits.emplace_back("MainDockSpace","UiPropertiesSpace",ImGuiDir_Right,0.27f);
        layout.dockingSplits.emplace_back("ProjectSpace","UiElementsSpace",ImGuiDir_Up,0.58f);
        layout.dockableWindows.emplace_back("###UI Elements","UiElementsSpace");
        layout.dockableWindows.emplace_back("###UI Properties",compact?"UiElementsSpace":"UiPropertiesSpace");
        layout.dockableWindows.emplace_back("###UI Canvas","MainDockSpace");
    }
    layout.dockableWindows.emplace_back("###Project","ProjectSpace");
    if(output) {
        layout.dockingSplits.emplace_back("MainDockSpace","OutputSpace",ImGuiDir_Down,0.26f);
        layout.dockableWindows.emplace_back("###Build output","OutputSpace");
    }
    return layout;
}

void ApplyNamedSplits(ImGuiID root,const HelloImGui::DockingParams& layout)
{
    std::map<HelloImGui::DockSpaceName,ImGuiID> ids{{"MainDockSpace",root}};
    for(const auto& split:layout.dockingSplits) {
        auto initial=ids.find(split.initialDock);
        if(initial==ids.end() || ids.contains(split.newDock) || !std::isfinite(split.ratio) || split.ratio<=0 || split.ratio>=1)
            throw std::runtime_error("Invalid Hello ImGui docking split: "+split.newDock);
        auto remaining=initial->second;
        const auto created=ImGui::DockBuilderSplitNode(remaining,split.direction,split.ratio,nullptr,&remaining);
        initial->second=remaining;ids.emplace(split.newDock,created);
        if(auto* node=ImGui::DockBuilderGetNode(created))node->SetLocalFlags(split.nodeFlags);
    }
    for(const auto& window:layout.dockableWindows) {
        const auto target=ids.find(window.dockSpaceName);
        if(target==ids.end())throw std::runtime_error("Unknown Hello ImGui window target: "+window.dockSpaceName);
        ImGui::DockBuilderDockWindow(window.label.c_str(),target->second);
    }
    ImGui::DockBuilderFinish(root);
}

void BuildDefault(ImGuiID id,int page,bool compact,bool output,ImGuiViewport& viewport)
{
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodePos(id,viewport.WorkPos);
    ImGui::DockBuilderSetNodeSize(id,viewport.WorkSize);
    ApplyNamedSplits(id,DefaultLayout(page,compact,output));
}

void PlaceOutput(ImGuiID root,int page)
{
    const auto* content=ImGui::FindWindowByName(page==1?"###Scene":page==2?"###ConcordScript":"###UI Canvas");
    auto* node=content?content->DockNode:nullptr;
    if(node && ImGui::DockNodeGetRootNode(node)->ID!=root)node=nullptr;
    if(!node)node=ImGui::DockBuilderGetCentralNode(root);
    if(!node)node=ImGui::DockBuilderGetNode(root);
    if(!node)return;
    while(node->ChildNodes[0])node=node->ChildNodes[0];
    auto center=node->ID;
    const auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,0.26f,nullptr,&center);
    ImGui::DockBuilderDockWindow("###Build output",bottom);
    ImGui::DockBuilderFinish(root);
}
}

struct WorkspaceLayouts::Impl {
    struct LayoutState {
        std::string ini;
        bool loaded=false,output=false,outputPlaced=false;
    };
    std::filesystem::path directory;
    std::array<LayoutState,6> layouts;
    int active=-1,lastPage=1;
    ImGuiID dockId=0;

    void Capture()
    {
        if(active<0 || !ImGui::GetCurrentContext())return;
        size_t size=0;const auto* ini=ImGui::SaveIniSettingsToMemory(&size);
        layouts[active].ini.assign(ini,size);layouts[active].loaded=true;
    }
    void Load(int index)
    {
        auto& layout=layouts[index];
        if(layout.loaded)return;
        const auto file=directory/(std::string(LayoutNames[index])+".ini");
        if(std::filesystem::is_regular_file(file) && std::filesystem::file_size(file)<=256*1024)layout.ini=ReadText(file);
        layout.loaded=true;
    }
    void Persist()
    {
        if(directory.empty())return;
        for(size_t i=0;i<layouts.size();++i)if(layouts[i].loaded && !layouts[i].ini.empty())
            WriteText(directory/(std::string(LayoutNames[i])+".ini"),layouts[i].ini);
        std::ostringstream state;state<<"CONCORD_LAYOUTS 2\n"<<lastPage<<'\n';
        for(const auto& layout:layouts)state<<layout.output<<' '<<layout.outputPlaced<<'\n';
        WriteText(directory/"Workspace.state",state.str());
    }
};

WorkspaceLayouts::WorkspaceLayouts():m_impl(std::make_unique<Impl>()){}
WorkspaceLayouts::~WorkspaceLayouts()=default;

void WorkspaceLayouts::Open(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    auto next=std::make_unique<Impl>();next->directory=directory;
    const auto stateFile=directory/"Workspace.state";
    if(std::filesystem::is_regular_file(stateFile)) {
        std::istringstream input(ReadText(stateFile));std::string magic;int version=0,page=0;
        bool valid=static_cast<bool>(input>>magic>>version>>page) && magic=="CONCORD_LAYOUTS" && (version==1 || version==2) && page>=1 && page<=(version==1?2:3);
        const size_t count=version==1?4:6;
        for(size_t i=0;i<count;++i) {
            auto& layout=next->layouts[i];
            int output=0,placed=0;
            valid=static_cast<bool>(input>>output>>placed) && (output==0 || output==1) && (placed==0 || placed==1) && valid;
            layout.output=output==1;layout.outputPlaced=placed==1;
        }
        input>>std::ws;valid=valid && input.eof();
        if(valid)next->lastPage=page;else next->layouts={};
    }
    m_impl=std::move(next);
    if(ImGui::GetCurrentContext())ImGui::GetIO().IniFilename=nullptr;
}

int WorkspaceLayouts::LastPage() const {return m_impl->lastPage;}

void WorkspaceLayouts::Draw(int page,bool compact,bool& showOutput,bool restoreDefault,bool revealOutput)
{
    if(m_impl->directory.empty())return;
    auto& state=*m_impl;const int next=LayoutIndex(page,compact);
    auto* viewport=ImGui::GetMainViewport();
    if(state.active!=next) {
        if(state.active>=0) {state.layouts[state.active].output=showOutput;Save();}
        state.Load(next);
        state.active=next;state.lastPage=page;
        state.dockId=ImHashStr((std::string("Concord/Workspace/v3/")+LayoutNames[next]).c_str());
        auto& layout=state.layouts[next];showOutput=layout.output || revealOutput;
        ImGui::ClearIniSettings();
        if(!restoreDefault && ValidDockingIni(layout.ini,state.dockId))ImGui::LoadIniSettingsFromMemory(layout.ini.data(),layout.ini.size());
        if(restoreDefault || !ImGui::DockBuilderGetNode(state.dockId)) {
            ImGui::ClearIniSettings();BuildDefault(state.dockId,page,compact,showOutput,*viewport);
            layout.outputPlaced=showOutput;
        }
    } else if(restoreDefault) {
        ImGui::ClearIniSettings();BuildDefault(state.dockId,page,compact,showOutput,*viewport);
        state.layouts[next].outputPlaced=showOutput;
    }
    auto& layout=state.layouts[next];
    const bool visibilityChanged=layout.output!=showOutput;
    layout.output=showOutput;
    ImGui::DockSpaceOverViewport(state.dockId,viewport);
    if(showOutput && !layout.outputPlaced) {PlaceOutput(state.dockId,page);layout.outputPlaced=true;}
    if(restoreDefault || visibilityChanged || ImGui::GetIO().WantSaveIniSettings) {
        Save();ImGui::GetIO().WantSaveIniSettings=false;
    }
}

void WorkspaceLayouts::Save()
{
    m_impl->Capture();m_impl->Persist();
}
}
