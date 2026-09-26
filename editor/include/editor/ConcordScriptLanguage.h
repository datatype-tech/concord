// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORCONCORDSCRIPTLANGUAGE_H
#define CONCORD_EDITORCONCORDSCRIPTLANGUAGE_H

#include <string>
#include <string_view>
#include <vector>

namespace Concord::Editor {

/** Fast, dependency-free completion index for the active ConcordScript buffer. */
std::vector<std::string> ConcordScriptCompletions(std::string_view source,
                                                  std::string_view prefix);

} // namespace Concord::Editor
#endif // CONCORD_EDITORCONCORDSCRIPTLANGUAGE_H
