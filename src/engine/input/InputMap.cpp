// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/input/InputMap.h"

#include <algorithm>

namespace Concord {

void InputMap::BindKey(u32 action, KeyCode key)
{
    if (key == KeyCode::None) return;
    std::vector<InputBinding>& bindings = m_bindings[action];
    const InputBinding binding{Device::Key, static_cast<u32>(key)};
    if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end()) {
        bindings.push_back(binding);
    }
}

void InputMap::BindMouseButton(u32 action, MouseButton button)
{
    std::vector<InputBinding>& bindings = m_bindings[action];
    const InputBinding binding{Device::MouseButton, static_cast<u32>(button)};
    if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end()) {
        bindings.push_back(binding);
    }
}

void InputMap::UnbindKey(u32 action, KeyCode key)
{
    const auto it = m_bindings.find(action);
    if (it == m_bindings.end()) return;
    const InputBinding binding{Device::Key, static_cast<u32>(key)};
    std::erase_if(it->second, [&](const InputBinding& entry) { return entry == binding; });
    if (it->second.empty()) m_bindings.erase(action);
}

void InputMap::UnbindMouseButton(u32 action, MouseButton button)
{
    const auto it = m_bindings.find(action);
    if (it == m_bindings.end()) return;
    const InputBinding binding{Device::MouseButton, static_cast<u32>(button)};
    std::erase_if(it->second, [&](const InputBinding& entry) { return entry == binding; });
    if (it->second.empty()) m_bindings.erase(action);
}

void InputMap::ClearAction(u32 action)
{
    m_bindings.erase(action);
    m_states.erase(action);
}

void InputMap::Clear()
{
    m_bindings.clear();
    m_states.clear();
}

void InputMap::Update(const InputSnapshot& snapshot)
{
    const auto evaluate = [&](u32 action, bool down) {
        const bool wasDown = (m_states[action] & kInputStateDown) != 0;
        u8 flags = down ? kInputStateDown : u8{0};
        if (down && !wasDown) flags |= kInputStatePressed;
        if (!down && wasDown) flags |= kInputStateReleased;
        m_states[action] = flags;
    };

    for (auto& [action, bindings] : m_bindings) {
        bool down = false;
        for (const InputBinding& binding : bindings) {
            const u8 state = binding.device == Device::Key
                                 ? snapshot.keys[binding.code]
                                 : snapshot.mouseButtons[binding.code];
            if (state & kInputStateDown) {
                down = true;
                break;
            }
        }
        evaluate(action, down);
    }

    // Actions whose bindings were all removed keep their state so a rebind
    // preserves continuity; they report one release, then rest quietly.
    for (auto it = m_states.begin(); it != m_states.end();) {
        if (m_bindings.contains(it->first)) {
            ++it;
            continue;
        }
        evaluate(it->first, false);
        if ((m_states[it->first] & (kInputStateDown | kInputStateReleased)) == 0) {
            it = m_states.erase(it);
        } else {
            ++it;
        }
    }
}

bool InputMap::IsDown(u32 action) const noexcept
{
    const auto it = m_states.find(action);
    return it != m_states.end() && (it->second & kInputStateDown) != 0;
}

bool InputMap::WasPressed(u32 action) const noexcept
{
    const auto it = m_states.find(action);
    return it != m_states.end() && (it->second & kInputStatePressed) != 0;
}

bool InputMap::WasReleased(u32 action) const noexcept
{
    const auto it = m_states.find(action);
    return it != m_states.end() && (it->second & kInputStateReleased) != 0;
}

bool InputMap::IsBound(u32 action) const noexcept
{
    return m_bindings.contains(action);
}

} // namespace Concord
