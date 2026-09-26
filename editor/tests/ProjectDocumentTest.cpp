// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/ProjectDocument.h"
#include "editor/SceneDocument.h"
#include <windows.h>
#include <winioctl.h>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Concord::Editor;
namespace {
void Require(bool condition,const char* message)
{
    if(!condition)throw std::runtime_error(message);
}
template<class Fn> void Reject(Fn action,const char* message)
{
    bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,message);
}
/** A local directory junction does not need symbolic-link privileges on Windows. */
void Junction(const std::filesystem::path& link,const std::filesystem::path& target)
{
    struct MountPoint {
        DWORD tag;WORD length,reserved,substituteOffset,substituteLength,printOffset,printLength;
        WCHAR text[1];
    };
    // NT reparse targets use native separators; unlike Win32 file APIs, they do not normalize '/'.
    const auto destination=std::filesystem::absolute(target).lexically_normal().make_preferred().wstring();
    const std::wstring substitute=L"\\??\\"+destination;
    const auto textBytes=(substitute.size()+destination.size()+2)*sizeof(wchar_t);
    std::vector<unsigned char> storage(offsetof(MountPoint,text)+textBytes);
    auto* data=reinterpret_cast<MountPoint*>(storage.data());
    data->tag=IO_REPARSE_TAG_MOUNT_POINT;data->length=static_cast<WORD>(storage.size()-8);
    data->substituteLength=static_cast<WORD>(substitute.size()*sizeof(wchar_t));
    data->printOffset=data->substituteLength+sizeof(wchar_t);data->printLength=static_cast<WORD>(destination.size()*sizeof(wchar_t));
    std::memcpy(data->text,substitute.c_str(),data->substituteLength+sizeof(wchar_t));
    std::memcpy(reinterpret_cast<unsigned char*>(data->text)+data->printOffset,destination.c_str(),data->printLength+sizeof(wchar_t));
    std::filesystem::create_directory(link);
    const HANDLE handle=CreateFileW(link.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    Require(handle!=INVALID_HANDLE_VALUE,"Cannot open junction fixture");DWORD returned=0;
    const BOOL okay=DeviceIoControl(handle,FSCTL_SET_REPARSE_POINT,data,static_cast<DWORD>(storage.size()),nullptr,0,&returned,nullptr);
    const DWORD error=GetLastError();CloseHandle(handle);
    if(!okay)throw std::runtime_error("Cannot create junction fixture: "+std::to_string(error));
    const HANDLE followed=CreateFileW(link.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                     nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    Require(followed!=INVALID_HANDLE_VALUE,"Junction fixture target cannot be followed");CloseHandle(followed);
}
}

int main(int argc,char** argv)
{
    try {
        ProjectDocument project;project.name="Project \"quoted\" \\ title";project.startupUi="UI/Main.yu";
        project.decorated=false;project.fullscreen=true;project.width=1920;project.height=1080;project.targetFps=0;
        ProjectDocument loaded;loaded.Parse(project.Serialize());
        Require(loaded.Serialize()==project.Serialize(),"Project config failed to round trip");
        loaded.Parse("CONCORD_PROJECT 1\nname \"Minimal\"\nstartupScene \"Main.scene\"\nwindow 1280 720 1 1 0 1\n");
        Require(loaded.targetFps==120 && loaded.startupUi.empty(),"Optional config fields lost defaults");
        for(const auto* invalid:{"CONCORD_PROJECT 2\n","CONCORD_PROJECT 1\nname \"Missing fields\"\n",
            "CONCORD_PROJECT 1\nname \"A\"\nname \"B\"\n",
            "CONCORD_PROJECT 1\nname \"A\"\nstartupScene \"Main.scene\"\nwindow 1280 720 2 1 0 1\n",
            "CONCORD_PROJECT 1\nname \"A\"\nstartupScene \"Main.scene\"\nwindow 1280 720 1 1 0 1\ntargetFps -1\n"}) {
            const auto before=loaded.Serialize();Reject([&]{loaded.Parse(invalid);},"Malformed project was accepted");
            Require(loaded.Serialize()==before,"Failed project load modified live settings");
        }
        const auto parent=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"ConcordProjectTests";
        const auto test=parent/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root=test/"Project",outside=test/"Outside";
        std::filesystem::create_directories(root/"Scenes");std::filesystem::create_directories(root/"UI");std::filesystem::create_directories(outside);
        WriteText(root/"Scenes/Main.scene",SceneDocument::Starter().Serialize());WriteText(root/"UI/Main.yu","UI fixture");
        WriteText(outside/"External.scene","Outside fixture");project.ValidatePaths(root,true);
        Require(std::filesystem::equivalent(ResolveProjectPath(root,"Scenes/Main.scene",true),root/"Scenes/Main.scene"),"Valid startup scene did not resolve");
        const auto genericRoot=std::filesystem::path(std::filesystem::absolute(root).generic_wstring());
        Require(std::filesystem::equivalent(ResolveProjectPath(genericRoot,"Scenes/Main.scene",true),root/"Scenes/Main.scene"),
                "Absolute project root using CMake forward slashes did not resolve");
        Require(!std::filesystem::exists(ResolveProjectPath(root,"Scenes/New.scene",false)),"New safe path could not resolve");
        for(const auto* invalid:{"../External.scene","Scenes/../../External.scene","/absolute.scene","C:\\outside.scene","\\\\server\\file.scene",
                                "Scenes/","Scenes//Main.scene","NUL.scene","Scenes/COM1.cx","Scenes/Main.scene:stream","bad./Main.scene","bad /Main.scene","."})
            Reject([&]{ResolveProjectPath(root,invalid);},"Unsafe lexical project path was accepted");
        Reject([&]{ResolveProjectPath(root,"Missing.scene",true);},"Missing required scene was accepted");
        Reject([&]{ResolveProjectPath(root,"Scenes",false);},"Resource directory was accepted as a file");
        Junction(root/"ExternalLink",std::filesystem::path(std::filesystem::absolute(outside).generic_wstring()));
        Reject([&]{ResolveProjectPath(root,"ExternalLink/External.scene",true);},"Junction escaped the project root");
        Reject([&]{ResolveProjectPath(root,"ExternalLink/New.scene",false);},"Nonexistent destination escaped through a junction");
        Junction(root/"InternalLink",std::filesystem::path(std::filesystem::absolute(root/"Scenes").generic_wstring()));
        Require(std::filesystem::equivalent(ResolveProjectPath(root,"InternalLink/Main.scene",true),root/"Scenes/Main.scene"),"Internal junction was incorrectly rejected");
        project.startupScene="Scenes/Main.cx";Reject([&]{project.Serialize();},"Incorrect startup scene extension was accepted");
        project.startupScene="Scenes/Main.scene";project.width=0;Reject([&]{project.Serialize();},"Invalid resolution was accepted");
        std::cout<<"Project config round trip, optional fields, transactional validation, native/CMake paths and junction containment passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
