// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORBUILDPROCESS_H
#define CONCORD_EDITORBUILDPROCESS_H
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace Concord::Editor {
/** Background CLI execution. A Windows job owns only this editor's child tree. */
class BuildProcess {
public:
    ~BuildProcess();
    void Start(const std::filesystem::path& executable,const std::vector<std::wstring>& arguments,
               const std::filesystem::path& workingDirectory);
    void Stop();
    bool Busy() const { return m_busy.load(); }
    int ExitCode() const { return m_exitCode.load(); }
    std::string Output() const;
private:
    void Append(const std::string& text);
    std::thread m_worker;
    std::atomic<bool> m_busy=false,m_stop=false;
    std::atomic<int> m_exitCode=-1;
    mutable std::mutex m_mutex;
    std::string m_output;
};
}
#endif
