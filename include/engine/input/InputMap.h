// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_INPUT_INPUTMAP_H
#define CONCORD_INPUT_INPUTMAP_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/input/InputSnapshot.h"

#include <unordered_map>
#include <vector>

namespace Concord {

/**
 * Maps game-defined actions onto keyboard and mouse bindings.
 *
 * Actions are caller-owned u32 identifiers (an application enum cast to u32);
 * each can carry any number of key and mouse bindings whose inputs OR
 * together. Bindings may be added, removed, or replaced between frames, so a
 * settings screen can rebind controls while the game is running.
 *
 * Update() evaluates every bound action against one input pump's snapshot.
 * Edge queries use action-level state — an action is "pressed" on the pump
 * where it transitioned from released to down, no matter which of its
 * bindings caused the transition.
 */
class CENGINE_API InputMap {
public:
    /** Adds a keyboard binding for `action`; duplicate bindings are ignored. */
    void BindKey(u32 action, KeyCode key);
    /** Adds a mouse-button binding for `action`; duplicates are ignored. */
    void BindMouseButton(u32 action, MouseButton button);
    /** Removes one keyboard binding; the action survives its last binding. */
    void UnbindKey(u32 action, KeyCode key);
    /** Removes one mouse-button binding; the action survives its last binding. */
    void UnbindMouseButton(u32 action, MouseButton button);
    /** Drops every binding and recorded state of `action`. */
    void ClearAction(u32 action);
    /** Drops all bindings and recorded state. */
    void Clear();

    /** Evaluates every bound action against one input pump's snapshot. */
    void Update(const InputSnapshot& snapshot);

    /** Whether any binding of `action` is currently held down. */
    [[nodiscard]] bool IsDown(u32 action) const noexcept;
    /** Whether `action` transitioned into Down on the most recent Update. */
    [[nodiscard]] bool WasPressed(u32 action) const noexcept;
    /** Whether `action` transitioned out of Down on the most recent Update. */
    [[nodiscard]] bool WasReleased(u32 action) const noexcept;
    /** Whether `action` carries at least one binding. */
    [[nodiscard]] bool IsBound(u32 action) const noexcept;

private:
    enum class Device : u8 {
        Key,
        MouseButton,
    };

    struct InputBinding {
        Device device = Device::Key;
        u32 code = 0;
        [[nodiscard]] bool operator==(const InputBinding&) const noexcept = default;
    };

    std::unordered_map<u32, std::vector<InputBinding>> m_bindings;
    /** Per-action kInputState* flags from the most recent Update. */
    std::unordered_map<u32, u8> m_states;
};

} // namespace Concord

#endif // CONCORD_INPUT_INPUTMAP_H
