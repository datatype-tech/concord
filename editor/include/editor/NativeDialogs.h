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
/**
 * Native Windows file picker limited to one pattern such as L"*.scene".
 * @param folder Initial folder; ignored when it does not exist.
 */
std::filesystem::path ChooseFile(const wchar_t* title,const wchar_t* filterName,const wchar_t* pattern,const std::filesystem::path& folder);
/** UTF-8 UI strings are distinct from Windows native wide paths. */
std::filesystem::path Utf8Path(const std::string& text);
std::string Utf8Text(const std::filesystem::path& path);
/** Converts UTF-8 interface text for native dialog titles. */
std::wstring WideText(const std::string& text);
/** Launches an independent editor process; an empty project opens the project manager. */
void LaunchWorkspace(const std::filesystem::path& project,const std::filesystem::path& cli,const std::filesystem::path& sdk);
/** Opens the project folder in Windows Explorer. */
void RevealFolder(const std::filesystem::path& path);
/** Opens a document or web address with its registered Windows handler. */
void OpenExternal(const std::wstring& target);
}
#endif
