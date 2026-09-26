// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CUiDocument.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace Concord {
namespace {
bool InRange(float value, float low, float high)
{
    return std::isfinite(value) && value >= low && value <= high;
}

bool ValidId(const std::string& value)
{
    if (value.empty() || value.size() > 96) return false;
    auto letter = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (!letter(static_cast<unsigned char>(value.front()))) return false;
    for (unsigned char c : value)
        if (!letter(c) && !(c >= '0' && c <= '9') && c != '-' && c != '.') return false;
    return true;
}

void CheckText(const std::string& text, usize limit)
{
    if (text.size() > limit || text.find('\0') != std::string::npos)
        throw std::runtime_error("UI text is too long or contains a null byte");
}
}

void UiDocument::Validate() const
{
    if (!InRange(referenceSize.x, 1, 100000) || !InRange(referenceSize.y, 1, 100000) || elements.size() > 2048)
        throw std::runtime_error("UI reference size or element count is out of range");
    std::unordered_map<std::string, usize> indices;
    usize textBytes = 0;
    for (usize index = 0; index < elements.size(); ++index) {
        const auto& element = elements[index];
        if (!ValidId(element.id) || (!element.parent.empty() && !ValidId(element.parent)) ||
            !indices.emplace(element.id, index).second)
            throw std::runtime_error("UI elements require unique valid identifiers");
        if (element.kind < UiElementKind::Panel || element.kind > UiElementKind::Progress ||
            element.action < UiAction::None || element.action > UiAction::CloseWindow)
            throw std::runtime_error("Unsupported UI element kind or action");
        if (element.action != UiAction::None && element.kind != UiElementKind::Button)
            throw std::runtime_error("Window actions are only supported on buttons");
        if (!InRange(element.position.x, -1000000, 1000000) || !InRange(element.position.y, -1000000, 1000000) ||
            !InRange(element.size.x, 1, 100000) || !InRange(element.size.y, 1, 100000) ||
            !InRange(element.anchor.x, 0, 1) || !InRange(element.anchor.y, 0, 1) ||
            !InRange(element.rounding, 0, 4096) || !InRange(element.fontScale, 0.25f, 8) ||
            !InRange(element.value, 0, 1))
            throw std::runtime_error("UI geometry, font scale or value is out of range");
        CheckText(element.text, 16384);
        CheckText(element.input, 4096);
        textBytes += element.id.size() + element.parent.size() + element.text.size() + element.input.size();
    }
    if (textBytes > 3 * 1024 * 1024) throw std::runtime_error("UI document text exceeds its limit");

    const usize root = elements.size();
    std::vector<usize> parents(elements.size(), root);
    for (usize index = 0; index < elements.size(); ++index) {
        const auto& parent = elements[index].parent;
        if (parent.empty()) continue;
        const auto found = indices.find(parent);
        if (found == indices.end() || elements[found->second].kind != UiElementKind::Panel)
            throw std::runtime_error("UI parent must identify an existing panel");
        parents[index] = found->second;
    }
    std::vector<int> state(elements.size()), depth(elements.size());
    for (usize start = 0; start < elements.size(); ++start) {
        if (state[start] == 2) continue;
        std::vector<usize> path;
        usize current = start;
        while (current != root && state[current] != 2) {
            if (state[current] == 1) throw std::runtime_error("UI hierarchy contains a cycle");
            state[current] = 1;
            path.push_back(current);
            if (path.size() > 64) throw std::runtime_error("UI hierarchy is deeper than 64 levels");
            current = parents[current];
        }
        int parentDepth = current == root ? 0 : depth[current];
        for (auto position = path.rbegin(); position != path.rend(); ++position) {
            if (++parentDepth > 64) throw std::runtime_error("UI hierarchy is deeper than 64 levels");
            depth[*position] = parentDepth;
            state[*position] = 2;
        }
    }
}

std::vector<UiElementLayout> UiDocument::ResolveLayout(const UiDrawDesc& desc) const
{
    Validate();
    if (!InRange(desc.position.x, -10000000, 10000000) || !InRange(desc.position.y, -10000000, 10000000) ||
        !InRange(desc.size.x, 0, 100000) || !InRange(desc.size.y, 0, 100000))
        throw std::runtime_error("UI draw rectangle is invalid");
    const float scale = std::min(desc.size.x / referenceSize.x, desc.size.y / referenceSize.y);
    std::vector<UiElementLayout> result(elements.size());
    std::vector<bool> resolved(elements.size());
    std::unordered_map<std::string, usize> indices;
    for (usize index = 0; index < elements.size(); ++index) indices.emplace(elements[index].id, index);
    const UiElementLayout root{elements.size(), desc.position, desc.size, desc.position, desc.size, true, true};
    std::function<void(usize)> resolve = [&](usize index) {
        if (resolved[index]) return;
        const auto& element = elements[index];
        const UiElementLayout* parent = &root;
        if (!element.parent.empty()) {
            const usize parentIndex = indices.at(element.parent);
            resolve(parentIndex);
            parent = &result[parentIndex];
        }
        auto& layout = result[index];
        layout.index = index;
        layout.size = element.size * scale;
        layout.position = parent->position + element.position * scale;
        layout.position.x += element.anchor.x * (parent->size.x - layout.size.x);
        layout.position.y += element.anchor.y * (parent->size.y - layout.size.y);
        layout.clipPosition = {std::max(layout.position.x, parent->clipPosition.x),
                               std::max(layout.position.y, parent->clipPosition.y)};
        const Vec2 end{std::min(layout.position.x + layout.size.x, parent->clipPosition.x + parent->clipSize.x),
                       std::min(layout.position.y + layout.size.y, parent->clipPosition.y + parent->clipSize.y)};
        layout.clipSize = {std::max(0.0f, end.x - layout.clipPosition.x), std::max(0.0f, end.y - layout.clipPosition.y)};
        layout.visible = parent->visible && element.visible && layout.clipSize.x > 0 && layout.clipSize.y > 0;
        layout.enabled = parent->enabled && element.enabled;
        resolved[index] = true;
    };
    for (usize index = 0; index < elements.size(); ++index) resolve(index);
    return result;
}
}
