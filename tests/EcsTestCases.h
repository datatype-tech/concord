// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ECS_TEST_CASES_H
#define CONCORD_ECS_TEST_CASES_H

namespace ConcordTests {

/** Runs registry, world, and query regression checks. */
bool RunWorldEcsTests();

/** Runs structural-mutation and const-query checks. */
bool RunQueryEcsTests();

/** Runs deferred structural-command checks. */
bool RunDeferredEcsTests();

/** Runs scene, handle, and archetype rollback regression checks. */
bool RunSceneEcsTests();

} // namespace ConcordTests

#endif // CONCORD_ECS_TEST_CASES_H
