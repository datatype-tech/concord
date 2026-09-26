// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORNATIVEDIALOGS_H
#define CONCORD_EDITORNATIVEDIALOGS_H
#include <filesystem>
#include <string>
namespace Concord::Editor {
/** Native Windows folder picker; cancellation returns an empty path. */
std::filesystem::path ChooseFolder(const wchar_t* title);
/** Native Windows executable picker for advanced toolchain settings. */
std::filesystem::path ChooseExecutable();
/** UTF-8 UI strings are distinct from Windows native wide paths. */
std::filesystem::path Utf8Path(const std::string& text);
std::string Utf8Text(const std::filesystem::path& path);
/** Launches an independent editor process; an empty project opens the project manager. */
void LaunchWorkspace(const std::filesystem::path& project,const std::filesystem::path& cli,const std::filesystem::path& sdk);
/** Opens the project folder in Windows Explorer. */
void RevealFolder(const std::filesystem::path& path);
}
#endif
