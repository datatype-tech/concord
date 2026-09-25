// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_CLISTYLE_H
#define CONCORDSCRIPT_CLISTYLE_H

/**
 * Terminal presentation for the ConcordScript CLIs.
 *
 * Diagnostics carry no colour: this layer paints them when the output is a
 * real console. Redirected CI logs, --color never, and NO_COLOR stay plain
 * text, so tests that match a message body keep working.
 */

#include <cstdio>
#include <string>
#include <string_view>

namespace ConcordScript {

enum class ColorMode { Auto, Always, Never };

enum class Style {
    Reset,
    Bold,
    Dim,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    BoldRed,
    BoldGreen,
    BoldYellow,
    BoldBlue,
    BoldMagenta,
    BoldCyan,
    BoldWhite,
};

/** Remembers argv[0]'s file name so help and errors name the tool that ran. */
void SetToolName(std::string_view argv0);

[[nodiscard]] std::string_view ToolName();

/** Parses auto/always/never. Returns false when \p text is not one of those. */
[[nodiscard]] bool ParseColorMode(std::string_view text, ColorMode& mode);

/**
 * Installs the colour policy and, on Windows, turns on virtual-terminal
 * sequences so a console actually honours them.
 */
void ConfigureColor(ColorMode mode);

/** Whether ANSI codes should be written to \p stream under the current policy. */
[[nodiscard]] bool ColorEnabled(std::FILE* stream);

/** ANSI for \p style, or empty when colour is off for \p stream. */
[[nodiscard]] std::string_view Ansi(Style style, std::FILE* stream);

/** Writes \p text in \p style, then resets if anything was emitted. */
void WriteStyled(std::FILE* stream, Style style, std::string_view text);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_CLISTYLE_H
