// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/ProjectDocument.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Concord::Editor {
namespace {
std::string Lower(std::string value)
{
    for(auto& c:value)if(static_cast<unsigned char>(c)<128)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
std::filesystem::path RelativePath(const std::string& value)
{
    if(value.empty() || value.size()>1024)throw std::runtime_error("Project path must contain between 1 and 1024 bytes");
    std::string normalized=value;
    for(auto& c:normalized) {
        if(static_cast<unsigned char>(c)<32 || c==127 || std::string_view(":*?\"<>|").find(c)!=std::string_view::npos)
            throw std::runtime_error("Project path contains an invalid Windows filename character");
        if(c=='\\')c='/';
    }
    if(normalized.front()=='/' || normalized.back()=='/')throw std::runtime_error("Project path must be a relative filename");
    std::istringstream parts(normalized);std::string part;
    while(std::getline(parts,part,'/')) {
        if(part.empty() || part=="." || part==".." || part.back()=='.' || part.back()==' ')
            throw std::runtime_error("Project paths cannot contain traversal, empty components or trailing dots/spaces");
        const auto stem=Lower(part.substr(0,part.find('.')));
        if(stem=="con" || stem=="prn" || stem=="aux" || stem=="nul" || stem=="conin$" || stem=="conout$" ||
           (stem.size()==4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3]>='1' && stem[3]<='9'))
            throw std::runtime_error("Project paths cannot use Windows device names");
    }
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(normalized.data()),normalized.size()));
}
void ReadBoolean(std::istream& input,bool& result)
{
    int value=-1;if(!(input>>value) || (value!=0 && value!=1))throw std::runtime_error("Invalid project boolean; expected 0 or 1");
    result=value==1;
}
std::wstring ExtendedPath(const std::filesystem::path& path)
{
    const auto text=path.wstring();
    if(text.starts_with(L"\\\\?\\"))return text;
    if(text.starts_with(L"\\\\"))return L"\\\\?\\UNC\\"+text.substr(2);
    return L"\\\\?\\"+text;
}
/** MinGW filesystem canonical does not resolve NTFS junctions; ask the kernel for the followed handle path. */
std::filesystem::path CanonicalWindowsPath(const std::filesystem::path& input,bool allowMissing)
{
    auto cursor=std::filesystem::absolute(input).lexically_normal();std::vector<std::filesystem::path> missing;
    for(;;) {
        const auto extended=ExtendedPath(cursor);
        const HANDLE handle=CreateFileW(extended.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                       nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if(handle!=INVALID_HANDLE_VALUE) {
            const DWORD required=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
            if(!required){CloseHandle(handle);throw std::runtime_error("Cannot resolve project resource handle");}
            std::wstring text(required,L'\0');
            const DWORD length=GetFinalPathNameByHandleW(handle,text.data(),required,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
            CloseHandle(handle);
            if(!length || length>=required)throw std::runtime_error("Cannot read resolved project resource path");
            text.resize(length);
            if(text.starts_with(L"\\\\?\\UNC\\"))text=L"\\\\"+text.substr(8);
            else if(text.starts_with(L"\\\\?\\"))text.erase(0,4);
            auto resolved=std::filesystem::path(text);
            for(auto part=missing.rbegin();part!=missing.rend();++part)resolved/=*part;
            return resolved.lexically_normal();
        }
        const DWORD error=GetLastError();
        const DWORD attributes=GetFileAttributesW(extended.c_str());
        if(!allowMissing || (error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) ||
           (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT)))
            throw std::runtime_error("Cannot safely resolve project path (Windows error "+std::to_string(error)+")");
        const auto parent=cursor.parent_path();
        if(parent==cursor || parent.empty())throw std::runtime_error("Project path has no existing parent");
        missing.push_back(cursor.filename());cursor=parent;
    }
}
}

void ProjectDocument::Validate() const
{
    if(name.empty() || name.size()>160)throw std::runtime_error("Project name must contain between 1 and 160 bytes");
    for(unsigned char c:name)if(c<32 || c==127)throw std::runtime_error("Project name contains a control character");
    const auto scene=RelativePath(startupScene);
    const auto sceneExtension=Lower(scene.extension().string());
    if(sceneExtension!=".scene")throw std::runtime_error("Startup scene must use .scene");
    if(!startupUi.empty() && Lower(RelativePath(startupUi).extension().string())!=".yu")throw std::runtime_error("Startup UI must use .yu");
    if(width<320 || height<240 || width>16384 || height>16384)throw std::runtime_error("Project resolution must be within 320x240 and 16384x16384");
    if(targetFps>1000)throw std::runtime_error("Target FPS must be 0 (unbounded) or between 1 and 1000");
}

std::string ProjectDocument::Serialize() const
{
    Validate();std::ostringstream out;out.imbue(std::locale::classic());
    out<<"CONCORD_PROJECT 1\nname "<<std::quoted(name)<<"\nstartupScene "<<std::quoted(startupScene)
       <<"\nstartupUi "<<std::quoted(startupUi)<<"\nwindow "<<width<<' '<<height<<' '<<decorated<<' '<<resizable<<' '<<fullscreen<<' '<<vsync
       <<"\ntargetFps "<<targetFps<<'\n';
    return out.str();
}

void ProjectDocument::Parse(const std::string& text)
{
    if(text.size()>64*1024 || text.find('\0')!=std::string::npos)throw std::runtime_error("Project document is too large or contains a null byte");
    ProjectDocument parsed;std::istringstream input(text);input.imbue(std::locale::classic());
    std::string magic,key;int version=0;
    if(!(input>>magic>>version) || magic!="CONCORD_PROJECT" || version!=1)throw std::runtime_error("Unsupported or corrupt project header");
    std::set<std::string> seen;
    while(input>>key) {
        if(!seen.insert(key).second)throw std::runtime_error("Duplicate project field: "+key);
        if(key=="name")input>>std::quoted(parsed.name);
        else if(key=="startupScene")input>>std::quoted(parsed.startupScene);
        else if(key=="startupUi")input>>std::quoted(parsed.startupUi);
        else if(key=="window") {
            input>>parsed.width>>parsed.height;ReadBoolean(input,parsed.decorated);ReadBoolean(input,parsed.resizable);
            ReadBoolean(input,parsed.fullscreen);ReadBoolean(input,parsed.vsync);
        } else if(key=="targetFps") {
            unsigned long long value=0;if(!(input>>value) || value>1000)throw std::runtime_error("Invalid target FPS");
            parsed.targetFps=static_cast<unsigned int>(value);
        } else throw std::runtime_error("Unknown project field: "+key);
        if(!input)throw std::runtime_error("Incomplete project field: "+key);
    }
    if(!seen.contains("name") || !seen.contains("startupScene") || !seen.contains("window"))throw std::runtime_error("Project is missing name, startupScene or window settings");
    parsed.Validate();*this=std::move(parsed);
}

std::filesystem::path ResolveProjectPath(const std::filesystem::path& project,const std::string& relative,bool requireExisting)
{
    const auto local=RelativePath(relative);
    if(!std::filesystem::is_directory(project))throw std::runtime_error("Project root must be an existing directory");
    const auto root=CanonicalWindowsPath(project,false);
    const auto candidate=CanonicalWindowsPath(root/local,!requireExisting);
    auto actual=candidate.begin();
    for(const auto& component:root) {
        if(actual==candidate.end() || CompareStringOrdinal(component.c_str(),-1,actual->c_str(),-1,TRUE)!=CSTR_EQUAL)
            throw std::runtime_error("Project path resolves outside the project directory");
        ++actual;
    }
    if(actual==candidate.end())throw std::runtime_error("Project path must name a file, not the project root");
    if(std::filesystem::exists(candidate) && !std::filesystem::is_regular_file(candidate))throw std::runtime_error("Project resource path is not a regular file");
    if(requireExisting && !std::filesystem::is_regular_file(candidate))throw std::runtime_error("Project resource file does not exist: "+relative);
    return candidate;
}

void ProjectDocument::ValidatePaths(const std::filesystem::path& project,bool requireExisting) const
{
    Validate();ResolveProjectPath(project,startupScene,requireExisting);
    if(!startupUi.empty())ResolveProjectPath(project,startupUi,requireExisting);
}
}
