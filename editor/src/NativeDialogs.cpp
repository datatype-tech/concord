// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/NativeDialogs.h"
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <stdexcept>
namespace Concord::Editor {
namespace {
std::filesystem::path Choose(const wchar_t* title,bool folder)
{
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IFileOpenDialog* dialog=nullptr;std::filesystem::path result;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        DWORD options=0;dialog->GetOptions(&options);
        dialog->SetOptions(options|FOS_FORCEFILESYSTEM|(folder?FOS_PICKFOLDERS:FOS_FILEMUSTEXIST));dialog->SetTitle(title);
        COMDLG_FILTERSPEC filter{L"Windows executable",L"*.exe"};if(!folder)dialog->SetFileTypes(1,&filter);
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))) {
            IShellItem* item=nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR value=nullptr;
                if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&value))) {result=value;CoTaskMemFree(value);}
                item->Release();
            }
        }
        dialog->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();return result;
}
}
std::filesystem::path ChooseFolder(const wchar_t* title) {return Choose(title,true);}
std::filesystem::path ChooseExecutable() {return Choose(L"Select concord.exe",false);}
std::filesystem::path Utf8Path(const std::string& text) {return std::filesystem::path(reinterpret_cast<const char8_t*>(text.c_str()));}
std::string Utf8Text(const std::filesystem::path& path) {auto text=path.u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};}
void RevealFolder(const std::filesystem::path& path) {ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
void LaunchWorkspace(const std::filesystem::path& project,const std::filesystem::path& cli,const std::filesystem::path& sdk)
{
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    auto quote=[](const std::wstring& value) {
        std::wstring result=L"\"";size_t slashes=0;
        for(wchar_t c:value) {if(c==L'\\'){++slashes;continue;}result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;}
        result.append(slashes*2,L'\\');return result+L"\"";
    };
    auto command=quote(executable)+L" "+(project.empty()?L"--manager":quote(project.wstring()))+L" "+quote(cli.wstring())+L" "+quote(sdk.wstring());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))
        throw std::runtime_error("Cannot open workspace window");
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
}
}
