// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/BuildProcess.h"
#include <windows.h>
#include <chrono>
#include <stdexcept>
namespace Concord::Editor {
namespace {
struct Handle {
    HANDLE value=nullptr;
    ~Handle() { if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring Quote(const std::wstring& value)
{
    std::wstring result=L"\""; size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\') {++slashes;continue;}
        if(c==L'"') result.append(slashes*2+1,L'\\'); else result.append(slashes,L'\\');
        slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'"';return result;
}
}
BuildProcess::~BuildProcess() { Stop(); if(m_worker.joinable())m_worker.join(); }
void BuildProcess::Stop() { m_stop=true; }
void BuildProcess::Append(const std::string& text)
{
    std::lock_guard lock(m_mutex); m_output+=text;
    if(m_output.size()>1024*1024) m_output.erase(0,m_output.size()-1024*1024);
}
std::string BuildProcess::Output() const {std::lock_guard lock(m_mutex);return m_output;}
void BuildProcess::Start(const std::filesystem::path& executable,const std::vector<std::wstring>& arguments,
                         const std::filesystem::path& workingDirectory)
{
    if(Busy()) throw std::runtime_error("A build or game is already running");
    if(!std::filesystem::is_regular_file(executable)) throw std::runtime_error("CLI executable not found: "+executable.string());
    if(m_worker.joinable())m_worker.join();
    {std::lock_guard lock(m_mutex);m_output.clear();}
    m_stop=false;m_exitCode=-1;m_busy=true;
    m_worker=std::thread([this,executable,arguments,workingDirectory] {
        try {
            Handle read,write,job,process,thread,input;
            SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
            if(!CreatePipe(&read.value,&write.value,&security,0) || !SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0))
                throw std::runtime_error("Cannot create compiler output pipe");
            input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
            job.value=CreateJobObjectW(nullptr,nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if(!job.value || !SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))
                throw std::runtime_error("Cannot create process job");
            std::wstring command=Quote(executable.wstring());
            for(const auto& argument:arguments)command+=L" "+Quote(argument);
            STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
            startup.hStdOutput=write.value;startup.hStdError=write.value;startup.hStdInput=input.value;
            PROCESS_INFORMATION info{};
            if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,
                               nullptr,workingDirectory.c_str(),&startup,&info))
                throw std::runtime_error("Cannot start CLI (Windows error "+std::to_string(GetLastError())+")");
            process.value=info.hProcess;thread.value=info.hThread;
            if(!AssignProcessToJobObject(job.value,process.value)) {
                TerminateProcess(process.value,1);throw std::runtime_error("Cannot assign child to editor job");
            }
            ResumeThread(thread.value);CloseHandle(write.value);write.value=nullptr;
            bool exited=false;
            for(;;) {
                if(m_stop) TerminateJobObject(job.value,130);
                DWORD available=0;
                if(PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr) && available) {
                    char buffer[4096];DWORD received=0;
                    if(ReadFile(read.value,buffer,sizeof(buffer),&received,nullptr) && received) Append({buffer,received});
                } else if(exited) break;
                else std::this_thread::sleep_for(std::chrono::milliseconds(20));
                exited=WaitForSingleObject(process.value,0)==WAIT_OBJECT_0;
            }
            DWORD code=1;GetExitCodeProcess(process.value,&code);m_exitCode=static_cast<int>(code);
            Append("\n[Process finished: "+std::to_string(code)+"]\n");
        } catch(const std::exception& error) {m_exitCode=1;Append(std::string("\n[Error] ")+error.what()+"\n");}
        m_busy=false;
    });
}
}
