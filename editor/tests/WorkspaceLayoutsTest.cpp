// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/WorkspaceLayouts.h"
#include "editor/SceneDocument.h"
#include <imgui_internal.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition,const char* message)
{
    if(!condition)throw std::runtime_error(message);
}

class Fixture {
public:
    Fixture()
    {
        ImGui::CreateContext();auto& io=ImGui::GetIO();
        io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
        io.DisplaySize={1500,900};io.DeltaTime=1.0f/60.0f;
        unsigned char* pixels=nullptr;int width=0,height=0;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(1);
    }
    ~Fixture(){ImGui::DestroyContext();}
    void Frame(Concord::Editor::WorkspaceLayouts& layouts,int page,bool compact,bool& output,bool reset=false)
    {
        ImGui::GetIO().DisplaySize=compact?ImVec2{960,720}:ImVec2{1500,900};
        ImGui::NewFrame();layouts.Draw(page,compact,output,reset);
        Panel("###Project");
        if(page==1){Panel("###Hierarchy");Panel("###Inspector");Panel("###Scene");}
        else if(page==2)Panel("###ConcordScript");
        else {Panel("###UI Elements");Panel("###UI Properties");Panel("###UI Canvas");}
        if(output)Panel("###Build output");
        ImGui::Render();
    }
    static ImGuiID DockId(const char* name)
    {
        auto* window=ImGui::FindWindowByName(name);
        Require(window && window->DockId!=0,"Expected panel was not docked");return window->DockId;
    }
private:
    static void Panel(const char* name){ImGui::Begin(name);ImGui::TextUnformatted(name);ImGui::End();}
};
}

int main(int argc,char** argv)
{
    try {
        const auto parent=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"ConcordLayoutTests";
        const auto directory=parent/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        Fixture fixture;Concord::Editor::WorkspaceLayouts layouts;layouts.Open(directory);
        bool output=false;
        fixture.Frame(layouts,1,false,output);fixture.Frame(layouts,1,false,output);
        const auto hierarchy=Fixture::DockId("###Hierarchy");
        Require(Fixture::DockId("###Inspector")!=hierarchy,"Wide layout should have a separate inspector");
        ImGui::DockBuilderDockWindow("###Inspector",hierarchy);
        fixture.Frame(layouts,1,false,output);
        Require(Fixture::DockId("###Inspector")==hierarchy,"Fixture failed to customize Scene layout");
        layouts.Save();

        fixture.Frame(layouts,2,false,output);fixture.Frame(layouts,2,false,output);
        const auto project=Fixture::DockId("###Project");output=true;
        fixture.Frame(layouts,2,false,output);fixture.Frame(layouts,2,false,output);
        Require(Fixture::DockId("###Project")==project,"Showing output replaced the source sidebar layout");
        Require(Fixture::DockId("###Build output")!=Fixture::DockId("###ConcordScript"),"Output did not get its own panel");
        output=false;fixture.Frame(layouts,2,false,output);fixture.Frame(layouts,2,false,output);
        output=true;fixture.Frame(layouts,2,false,output);fixture.Frame(layouts,2,false,output);
        Require(Fixture::DockId("###Project")==project,"Toggling output destroyed the sidebar layout");

        fixture.Frame(layouts,1,false,output);fixture.Frame(layouts,1,false,output);
        Require(Fixture::DockId("###Inspector")==Fixture::DockId("###Hierarchy"),"Scene customization was lost after switching from Code");
        Require(!output,"Output visibility was not independent per workspace");
        fixture.Frame(layouts,1,true,output);fixture.Frame(layouts,1,true,output);
        Require(Fixture::DockId("###Inspector")==Fixture::DockId("###Hierarchy"),"Compact mode did not use tabbed inspector");
        fixture.Frame(layouts,1,false,output);fixture.Frame(layouts,1,false,output);
        Require(Fixture::DockId("###Inspector")==Fixture::DockId("###Hierarchy"),"Compact transition discarded the wide customization");
        fixture.Frame(layouts,1,false,output,true);fixture.Frame(layouts,1,false,output);
        Require(Fixture::DockId("###Inspector")!=Fixture::DockId("###Hierarchy"),"Restore default did not reset current layout");

        fixture.Frame(layouts,2,false,output);fixture.Frame(layouts,2,false,output);layouts.Save();
        Require(output,"Code output preference was lost after switching workspace");
        Concord::Editor::WorkspaceLayouts reopened;reopened.Open(directory);
        Require(reopened.LastPage()==2,"Last active workspace did not persist");
        bool restoredOutput=false;fixture.Frame(reopened,2,false,restoredOutput);fixture.Frame(reopened,2,false,restoredOutput);
        Require(restoredOutput,"Restart lost output visibility");
        Require(Fixture::DockId("###Project")==project,"Restart did not restore saved node identities");
        Require(std::filesystem::is_regular_file(directory/"SceneWide.ini") && std::filesystem::is_regular_file(directory/"SceneCompact.ini") &&
                std::filesystem::is_regular_file(directory/"CodeWide.ini"),"Named layout snapshots were not written");

        fixture.Frame(reopened,3,false,restoredOutput);fixture.Frame(reopened,3,false,restoredOutput);
        Require(Fixture::DockId("###UI Elements")!=Fixture::DockId("###UI Properties"),"Wide UI workspace did not separate element and property panels");
        const auto elements=Fixture::DockId("###UI Elements");ImGui::DockBuilderDockWindow("###UI Properties",elements);
        fixture.Frame(reopened,3,false,restoredOutput);
        fixture.Frame(reopened,2,false,restoredOutput);fixture.Frame(reopened,2,false,restoredOutput);
        Require(Fixture::DockId("###Project")==project,"UI workspace replaced the Code layout");
        fixture.Frame(reopened,1,false,restoredOutput);fixture.Frame(reopened,1,false,restoredOutput);
        Require(Fixture::DockId("###Inspector")!=Fixture::DockId("###Hierarchy"),"UI workspace replaced the Scene layout");
        fixture.Frame(reopened,3,false,restoredOutput);fixture.Frame(reopened,3,false,restoredOutput);
        Require(Fixture::DockId("###UI Properties")==Fixture::DockId("###UI Elements"),"UI customization was lost across all three workspaces");
        fixture.Frame(reopened,3,true,restoredOutput);fixture.Frame(reopened,3,true,restoredOutput);
        Require(Fixture::DockId("###UI Properties")==Fixture::DockId("###UI Elements"),"Compact UI workspace did not combine properties with elements");
        reopened.Save();Concord::Editor::WorkspaceLayouts uiRestart;uiRestart.Open(directory);
        Require(uiRestart.LastPage()==3,"UI was not restored as the startup workspace");
        Require(std::filesystem::is_regular_file(directory/"UIWide.ini") && std::filesystem::is_regular_file(directory/"UICompact.ini"),"UI snapshots were not written independently");

        const auto legacy=directory/"legacy";std::filesystem::create_directories(legacy);
        Concord::Editor::WriteText(legacy/"Workspace.state","CONCORD_LAYOUTS 1\n2\n0 0\n0 0\n1 0\n0 0\n");
        Concord::Editor::WorkspaceLayouts migrated;migrated.Open(legacy);bool legacyOutput=false;
        Require(migrated.LastPage()==2,"V1 workspace state lost its active page");
        fixture.Frame(migrated,2,false,legacyOutput);fixture.Frame(migrated,2,false,legacyOutput);
        Require(legacyOutput,"V1 state lost output visibility");migrated.Save();
        Require(Concord::Editor::ReadText(legacy/"Workspace.state").starts_with("CONCORD_LAYOUTS 2\n"),"V1 layout state was not upgraded");

        Concord::Editor::WriteText(directory/"SceneWide.ini","[Docking][Data]\nDockSpace ID=0x00000001 Parent=0x00000001 SizeRef=900,600\n");
        reopened.Open(directory);fixture.Frame(reopened,1,false,restoredOutput);fixture.Frame(reopened,1,false,restoredOutput);
        Require(Fixture::DockId("###Inspector")!=Fixture::DockId("###Hierarchy"),"Damaged docking data did not restore a usable default");
        std::cout<<"Hello ImGui Scene/Code/UI layouts, width persistence, output toggling, restart, v1 migration and damaged-layout recovery passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
