// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CliReport.h"

#include <cstdio>
#include <string>

namespace ConcordScript {
namespace {

void Labelled(std::FILE* stream, Style style, std::string_view label, std::string_view message)
{
    WriteStyled(stream, style, label);
    WriteStyled(stream, Style::Bold, ": ");
    std::fwrite(message.data(), 1, message.size(), stream);
    std::fputc('\n', stream);
    std::fflush(stream);
}

void Flag(std::string_view name, std::string_view argument, std::string_view help)
{
    std::FILE* out = stdout;
    std::fputs("    ", out);
    WriteStyled(out, Style::BoldGreen, name);
    if (!argument.empty()) {
        std::fputc(' ', out);
        WriteStyled(out, Style::Bold, argument);
    }
    const std::size_t used = 4 + name.size() + (argument.empty() ? 0 : 1 + argument.size());
    const std::size_t pad = used < 32 ? 32 - used : 2;
    for (std::size_t index = 0; index < pad; ++index) std::fputc(' ', out);
    std::fwrite(help.data(), 1, help.size(), out);
    std::fputc('\n', out);
}

} // namespace

void ReportError(std::string_view message)
{
    Labelled(stderr, Style::BoldRed, "error", message);
}

void ReportWarning(std::string_view message)
{
    Labelled(stderr, Style::BoldYellow, "warning", message);
}

void ReportNote(std::string_view message)
{
    Labelled(stderr, Style::BoldCyan, "note", message);
}

void ReportSuccess(std::string_view message)
{
    Labelled(stdout, Style::BoldGreen, ToolName(), message);
}

void ReportLocated(std::string_view path, int line, bool isError, std::string_view message)
{
    std::FILE* stream = stderr;
    WriteStyled(stream, isError ? Style::BoldRed : Style::BoldYellow, isError ? "error" : "warning");
    WriteStyled(stream, Style::Bold, ": ");
    WriteStyled(stream, Style::BoldCyan, path);
    std::fprintf(stream, ":%d: ", line);
    std::fwrite(message.data(), 1, message.size(), stream);
    std::fputc('\n', stream);
    std::fflush(stream);
}

void PrintHelp(bool defaultToCvm)
{
    std::FILE* out = stdout;
    const std::string_view tool = ToolName();

    WriteStyled(out, Style::BoldYellow, "USAGE");
    WriteStyled(out, Style::Bold, ":\n    ");
    WriteStyled(out, Style::BoldWhite, tool);
    std::fputs(" [OPTIONS] --project <DIR> --out <DIR>\n\n", out);

    WriteStyled(out, Style::BoldYellow, "OPTIONS");
    std::fputs(":\n", out);
    Flag("--project", "<DIR>", "Project root containing .cx files");
    Flag("--init", "<DIR>", "Create a new .cx game project (empty directory only)");
    Flag("--out", "<DIR>", "Directory for generated artifacts");
    Flag("--cpp", {}, "Generate C++ (.gen.h / .gen.cpp)");
    Flag("--check", {}, "Validate C++ generation without writing (no --out needed)");
    Flag("--cvm", "[jit|aot]", "Concord Visual Machine (LLVM)");
    Flag("--jit, --aot", {}, "Shorthand for --cvm jit / --cvm aot");
    Flag("--cvm-host", "<DLL>", "Load a host module (repeatable)");
    Flag("--run / --no-run", {}, "JIT-run main after compiling (CVM)");
    Flag("--color", "<MODE>", "auto | always | never");
    Flag("--no-color", {}, "Same as --color never");
    Flag("-h, --help", {}, "Show this help");
    Flag("-V, --version", {}, "Show the compiler version");

    std::fputc('\n', out);
    WriteStyled(out, Style::BoldYellow, "DEFAULT");
    WriteStyled(out, Style::Bold, ": ");
    if (defaultToCvm) {
        std::fputs("this binary is ", out);
        WriteStyled(out, Style::BoldCyan, "cc");
        std::fputs(", so --cvm jit is implied.\n", out);
    } else {
        std::fputs("this binary is ", out);
        WriteStyled(out, Style::BoldCyan, "concordc");
        std::fputs(", so --cpp is implied.\n", out);
    }

    std::fputc('\n', out);
    WriteStyled(out, Style::BoldYellow, "EXAMPLES");
    std::fputs(":\n    ", out);
    WriteStyled(out, Style::BoldWhite, tool);
    std::fputs(" --project game --out out\n    ", out);
    WriteStyled(out, Style::BoldWhite, tool);
    std::fputs(" --color always --help\n", out);

    WriteStyled(out, Style::Dim, "error");
    std::fputs(" is ", out);
    WriteStyled(out, Style::BoldRed, "bold red");
    std::fputs(", ", out);
    WriteStyled(out, Style::Dim, "warning");
    std::fputs(" is ", out);
    WriteStyled(out, Style::BoldYellow, "bold yellow");
    std::fputs(", ", out);
    WriteStyled(out, Style::Dim, "note");
    std::fputs(" is ", out);
    WriteStyled(out, Style::BoldCyan, "bold cyan");
    std::fputs(". Pipes and CI stay plain text.\n", out);
    std::fflush(out);
}

void PrintVersion()
{
#ifndef CONCORDSCRIPT_VERSION
#define CONCORDSCRIPT_VERSION "0.1.0"
#endif
    std::FILE* out = stdout;
    WriteStyled(out, Style::BoldWhite, ToolName());
    std::fputs(" ", out);
    WriteStyled(out, Style::BoldGreen, CONCORDSCRIPT_VERSION);
    std::fputs(" (", out);
    WriteStyled(out, Style::Cyan, "ConcordScript");
    std::fputs(")\n", out);
    std::fflush(out);
}

} // namespace ConcordScript
