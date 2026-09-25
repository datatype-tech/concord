// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDSCRIPT_ANNOTATIONTEXTTEMPLATES_H
#define CONCORDSCRIPT_ANNOTATIONTEXTTEMPLATES_H

#include <cstddef>
#include <string>

namespace ConcordScript::AnnotationTextTemplates {

/** Returns whether a less-than character begins a C++ template argument list. */
bool IsTemplateOpen(const std::string& text, std::size_t position);

} // namespace ConcordScript::AnnotationTextTemplates

#endif // CONCORDSCRIPT_ANNOTATIONTEXTTEMPLATES_H
