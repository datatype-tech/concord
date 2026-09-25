// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CliStyle.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <io.h>
#include <windows.h>

namespace ConcordScript {
namespace {

ColorMode g_mode = ColorMode::Auto;
std::string g_toolName = "concordc";
bool g_vtEnabled = false;

void EnableVirtualTerminal()
{
    if (g_vtEnabled) return;
    g_vtEnabled = true;
    const HANDLE handles[] = {GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
    for (HANDLE handle : handles) {
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE) continue;
        DWORD mode = 0;
        if (!GetConsoleMode(handle, &mode)) continue;
        SetConsoleMode(handle, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

[[nodiscard]] bool IsTty(std::FILE* stream)
{
    if (stream == nullptr) return false;
    const int fd = _fileno(stream);
    if (fd < 0) return false;
    return _isatty(fd) != 0;
}

[[nodiscard]] bool ForceColor()
{
    const char* value = std::getenv("CLICOLOR_FORCE");
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

[[nodiscard]] bool NoColor()
{
    return std::getenv("NO_COLOR") != nullptr;
}

} // namespace

void SetToolName(std::string_view argv0)
{
    if (argv0.empty()) return;
    const std::filesystem::path path{std::string(argv0)};
    std::string name = path.filename().string();
    if (name.size() > 4) {
        const std::string tail = name.substr(name.size() - 4);
        if (_stricmp(tail.c_str(), ".exe") == 0) name.resize(name.size() - 4);
    }
    if (!name.empty()) g_toolName = std::move(name);
}

std::string_view ToolName()
{
    return g_toolName;
}

bool ParseColorMode(std::string_view text, ColorMode& mode)
{
    if (text == "auto") {
        mode = ColorMode::Auto;
        return true;
    }
    if (text == "always") {
        mode = ColorMode::Always;
        return true;
    }
    if (text == "never") {
        mode = ColorMode::Never;
        return true;
    }
    return false;
}

void ConfigureColor(ColorMode mode)
{
    g_mode = mode;
    if (mode != ColorMode::Never) EnableVirtualTerminal();
}

bool ColorEnabled(std::FILE* stream)
{
    if (g_mode == ColorMode::Never) return false;
    if (g_mode == ColorMode::Always) return true;
    if (NoColor()) return false;
    if (ForceColor()) return true;
    return IsTty(stream);
}

std::string_view Ansi(Style style, std::FILE* stream)
{
    if (!ColorEnabled(stream)) return {};
    switch (style) {
        case Style::Reset: return "\x1b[0m";
        case Style::Bold: return "\x1b[1m";
        case Style::Dim: return "\x1b[2m";
        case Style::Red: return "\x1b[31m";
        case Style::Green: return "\x1b[32m";
        case Style::Yellow: return "\x1b[33m";
        case Style::Blue: return "\x1b[34m";
        case Style::Magenta: return "\x1b[35m";
        case Style::Cyan: return "\x1b[36m";
        case Style::BoldRed: return "\x1b[1;31m";
        case Style::BoldGreen: return "\x1b[1;32m";
        case Style::BoldYellow: return "\x1b[1;33m";
        case Style::BoldBlue: return "\x1b[1;34m";
        case Style::BoldMagenta: return "\x1b[1;35m";
        case Style::BoldCyan: return "\x1b[1;36m";
        case Style::BoldWhite: return "\x1b[1;37m";
    }
    return {};
}

void WriteStyled(std::FILE* stream, Style style, std::string_view text)
{
    const std::string_view code = Ansi(style, stream);
    if (!code.empty()) std::fputs(code.data(), stream);
    std::fwrite(text.data(), 1, text.size(), stream);
    if (!code.empty()) std::fputs(Ansi(Style::Reset, stream).data(), stream);
}

} // namespace ConcordScript
