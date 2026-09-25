// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_MODULENAME_H
#define CONCORDSCRIPT_MODULENAME_H

#include <string>

namespace ConcordScript::ModuleName {

/** Returns whether a source stem can be used as a C++ namespace identifier. */
bool IsValid(const std::string& name);

} // namespace ConcordScript::ModuleName

#endif // CONCORDSCRIPT_MODULENAME_H
