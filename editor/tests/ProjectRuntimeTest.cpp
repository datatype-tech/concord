// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/ProjectRuntime.h"
#include "editor/SceneDocument.h"
#include <Concord/CUiDocument.h>
#include <iostream>
#include <stdexcept>
using namespace Concord;
using namespace Concord::Editor;
int main(int argc,char** argv)
{
    try {
        if(argc<2)throw std::runtime_error("Expected a fixture project directory");
        const auto root=std::filesystem::path(argv[1]);
        std::filesystem::create_directories(root/"Scenes");std::filesystem::create_directories(root/"UI");
        ProjectDocument project;project.name="Concord UI Studio";project.startupScene="Scenes/Initial.scene";
        project.startupUi="UI/Hud.yu";project.width=1280;project.height=800;project.decorated=false;
        auto scene=SceneDocument::Starter();scene.objects[1].kind=SceneObjectKind::DynamicBox;
        scene.objects[1].transform.position.y=5;scene.environment.bloomIntensity=0.18f;
        WriteText(root/"Scenes/Initial.scene",scene.Serialize());
        auto other=SceneDocument::Starter();other.objects.resize(1);WriteText(root/"Scenes/Other.scene",other.Serialize());
        UiDocument ui;
        ui.elements.push_back({.id="panel",.text="",.kind=UiElementKind::Panel,.position={24,24},.size={370,340},.rounding=18});
        ui.elements.push_back({.id="title",.parent="panel",.text="Concord UI Studio",.kind=UiElementKind::Label,.position={20,16},.size={330,35},.fontScale=1.25f});
        ui.elements.push_back({.id="fullscreen",.parent="panel",.text="Fullscreen",.position={20,64},.size={158,44},.rounding=12,.action=UiAction::ToggleFullscreenWindow});
        ui.elements.push_back({.id="quit",.parent="panel",.text="Quit",.position={190,64},.size={158,44},.rounding=12,.action=UiAction::CloseWindow});
        ui.elements.push_back({.id="check",.parent="panel",.text="Example toggle",.kind=UiElementKind::Checkbox,.position={20,126},.size={330,36},.value=1});
        ui.elements.push_back({.id="slider",.parent="panel",.kind=UiElementKind::Slider,.position={20,174},.size={330,32},.value=0.6f});
        ui.elements.push_back({.id="input",.parent="panel",.text="Player name",.kind=UiElementKind::TextInput,.position={20,218},.size={330,40}});
        ui.elements.push_back({.id="progress",.parent="panel",.kind=UiElementKind::Progress,.position={20,278},.size={330,24},.value=0.72f});
        ui.Save(root/"UI/Hud.yu");WriteText(root/"Concord.project",project.Serialize());
        auto generated=ExportProjectRuntime(project,ui.Serialize(),scene.UsesPhysics());
        if(generated.find(".resolution={1280,800}")==std::string::npos || generated.find(".decorated=false")==std::string::npos ||
           generated.find("Add<Concord::PhysicsSystem>")==std::string::npos || generated.find("Fullscreen")==std::string::npos)
            throw std::runtime_error("Project startup options were lost during export");
        auto collision=ui;collision.elements[1].text=")CFYU\"";
        if(ExportProjectRuntime(project,collision.Serialize(),false).find("Add<Concord::PhysicsSystem>")!=std::string::npos)
            throw std::runtime_error("Physics was enabled for a non-physics project");
        WriteText(root/"Main.cx",ProjectEntryScript());WriteText(root/"SceneLayout.cx",scene.ExportScript());
        WriteText(root/"ProjectRuntime.cx",generated);
        if(!IsProjectEntryScript(ProjectEntryScript()) || IsProjectEntryScript("// use Project.ProjectRuntime;\n@entry { return 0; };"))
            throw std::runtime_error("Custom entry was mistaken for the managed entry");
        const auto prepared=PrepareProjectRuntime(root,project);
        if(prepared.sceneSource!=scene.ExportScript() || prepared.runtimeSource!=generated)
            throw std::runtime_error("Configured initial scene did not drive runtime generation");
        auto changed=prepared;changed.sceneSource+="\n";
        WriteText(root/"ProjectRuntime.cx","// Hand-authored project module\n");
        bool rejected=false;try{SaveProjectRuntime(root,changed);}catch(const std::exception&){rejected=true;}
        if(!rejected || ReadText(root/"SceneLayout.cx")!=prepared.sceneSource || ReadText(root/"ProjectRuntime.cx")!="// Hand-authored project module\n")
            throw std::runtime_error("Generation partially overwrote files before detecting a custom module");
        WriteText(root/"ProjectRuntime.cx",generated);
        WriteText(root/"UI/Invalid.yu","broken document");
        auto invalid=project;invalid.startupUi="UI/Invalid.yu";rejected=false;
        try{(void)PrepareProjectRuntime(root,invalid);}catch(const std::exception&){rejected=true;}
        if(!rejected || ReadText(root/"Concord.project")!=project.Serialize() || ReadText(root/"ProjectRuntime.cx")!=generated)
            throw std::runtime_error("Invalid startup resources changed saved project state");
        std::filesystem::remove(root/"UI/Invalid.yu");
        std::cout<<"Project startup, UI embedding and physics fixture exported to "<<root.string()<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
