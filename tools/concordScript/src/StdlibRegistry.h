// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_STDLIBREGISTRY_H
#define CONCORDSCRIPT_STDLIBREGISTRY_H

/**
 * The whitelist of `use std.xxx;` packages.
 *
 * A dotted `std.` path is not a project module: it maps onto a C++ standard
 * header, and only the names in this table are legal. Unknown packages are
 * rejected with a nearby suggestion rather than being turned into a quoted
 * include of a generated file that does not exist.
 */

#include <string>
#include <string_view>

namespace ConcordScript {

/**
 * Returns the C++ header name for \p target (for example "std.vector" →
 * "vector"), or empty when the package is not on the whitelist.
 */
std::string_view StdHeaderForPackage(std::string_view target);

/**
 * Returns a nearby whitelisted package, including the `std.` prefix, or
 * empty when nothing is close enough to suggest.
 */
std::string SuggestStdPackage(std::string_view target);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_STDLIBREGISTRY_H
