// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_ANNOTATIONTEXTLITERALS_H
#define CONCORDSCRIPT_ANNOTATIONTEXTLITERALS_H

#include <cstddef>
#include <string>

namespace ConcordScript::AnnotationTextLiterals {

/** Returns the end of a C++ raw string, or zero when `position` is not one. */
size_t RawStringEnd(const std::string& text, size_t position);

} // namespace ConcordScript::AnnotationTextLiterals

#endif // CONCORDSCRIPT_ANNOTATIONTEXTLITERALS_H
