// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "editor/ConcordScriptLanguage.h"

#include <algorithm>
#include <cassert>

int main()
{
    const auto matches = Concord::Editor::ConcordScriptCompletions(
        "var playerSpeed = 10; // playerComment\n\"playerString\"\nvar playerHealth = 5;",
        "player");
    const auto has = [&](const char* name) {
        return std::find(matches.begin(), matches.end(), name) != matches.end();
    };
    assert(has("playerSpeed") && has("playerHealth"));
    assert(!has("playerComment") && !has("playerString"));
    assert(Concord::Editor::ConcordScriptCompletions("", "").empty());
}
