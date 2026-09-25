// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CLIREPORT_H
#define CONCORDSCRIPT_CLIREPORT_H

#include "CliStyle.h"

#include <string_view>

namespace ConcordScript {

/** Bold red `error:` plus the message, on stderr. */
void ReportError(std::string_view message);

/** Bold yellow `warning:` plus the message, on stderr. */
void ReportWarning(std::string_view message);

/** Bold cyan `note:` plus the message, on stderr. */
void ReportNote(std::string_view message);

/** Bold green summary on stdout. */
void ReportSuccess(std::string_view message);

/** File-located diagnostic: `error:` / `warning:` then `path:line: message`. */
void ReportLocated(std::string_view path, int line, bool isError, std::string_view message);

/** Coloured usage for --help. \p defaultToCvm selects which backend is implied. */
void PrintHelp(bool defaultToCvm);

/** Tool name and version, on stdout. */
void PrintVersion();

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CLIREPORT_H
