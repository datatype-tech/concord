// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_EDITORLOCALIZATION_H
#define CONCORD_EDITORLOCALIZATION_H
#include <string>

namespace Concord::Editor {
/** Interface language preference; System follows the Windows display language. */
enum class LanguagePreference { System, English, Chinese };

/** Applies a preference and resolves System against the current Windows UI language. */
void SetLanguage(LanguagePreference preference);
[[nodiscard]] LanguagePreference CurrentLanguagePreference();
/** Whether strings currently resolve to Simplified Chinese. */
[[nodiscard]] bool IsChinese();
/**
 * Translates an English interface string. English source text is the key, so an
 * untranslated string still reads correctly and printf formats keep their order.
 */
[[nodiscard]] const char* Tr(const char* english);
/**
 * Translated label with a language-independent ImGui identity ("text###id"), for
 * windows, popups, tabs and headers whose docking or open state must survive a
 * language switch. The id defaults to the English text.
 */
[[nodiscard]] std::string TrId(const char* english, const char* id = nullptr);
/** Every translated string concatenated, so the font atlas can include each glyph it needs. */
[[nodiscard]] const char* TranslationGlyphs();
}
#endif
