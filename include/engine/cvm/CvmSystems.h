// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CVMSYSTEMS_H
#define CONCORD_CVMSYSTEMS_H

/**
 * The engine systems a script asked for, applied when the game starts.
 *
 * A script cannot register a system when it asks for one, because there is no
 * Game until ConcordCvmRunScene creates it. The requests are therefore recorded
 * here and applied at that moment.
 */

namespace Concord {
class Game;
class Window;
} // namespace Concord

namespace Concord::Cvm {

/** Registers every requested system on \p game. */
void ApplyRequestedSystems(Game& game, Window& window);

/** Forgets pointers into a Game that is about to die. */
void ClearLiveSystems();

} // namespace Concord::Cvm

#endif // CONCORD_CVMSYSTEMS_H
