#ifndef CONCORDSCRIPT_KEYWORDSUBSTITUTION_H
#define CONCORDSCRIPT_KEYWORDSUBSTITUTION_H

#include <string>

namespace ConcordScript {

/**
 * Rewrites `var` to `auto` and each per-member `pub`/`priv`/`prot` marker
 * to a C++ access-section label, everywhere inside a captured class or
 * @entry body.
 *
 * Members with no marker default to `pub` (see script/README.md), which is
 * why the body is prefixed with an implicit `public:` before scanning: the
 * first run of members, before any explicit marker, then falls under that
 * label exactly like an explicit one would.
 */
std::string SubstituteKeywords(const std::string& bodyText, bool isClassBody);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_KEYWORDSUBSTITUTION_H
