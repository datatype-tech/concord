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
    if (!InRange(referenceSize.x, 1, 100000) || !InRange(referenceSize.y, 1, 100000) || elements.size() > 2048 ||
        place < UiPlace::Free || place > UiPlace::Center ||
        !InRange(placePosition.x, -1000000, 1000000) || !InRange(placePosition.y, -1000000, 1000000) ||
        !InRange(placeSize.x, 0, 100000) || !InRange(placeSize.y, 0, 100000))
        throw std::runtime_error("UI reference size, placement or element count is out of range");
    std::unordered_map<std::string, usize> indices;
    usize textBytes = 0;
    for (usize index = 0; index < elements.size(); ++index) {
        const auto& element = elements[index];
        if (!ValidId(element.id) || (!element.parent.empty() && !ValidId(element.parent)) ||
            !indices.emplace(element.id, index).second)
            throw std::runtime_error("UI elements require unique valid identifiers");
        if (element.kind < UiElementKind::Panel || element.kind > UiElementKind::Progress ||
            element.action < UiAction::None || element.action > UiAction::CloseWindow ||
            element.place < UiPlace::Free || element.place > UiPlace::Center)
            throw std::runtime_error("Unsupported UI element kind, action or place");
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

UiDrawDesc UiDocument::Region(const UiDrawDesc& host) const
{
    const float widthScale = host.size.x / referenceSize.x;
    const float heightScale = host.size.y / referenceSize.y;
    const float fit = std::min(widthScale, heightScale);
    const float slotWidth = placeSize.x > 0 ? placeSize.x : referenceSize.x;
    const float slotHeight = placeSize.y > 0 ? placeSize.y : referenceSize.y;
    UiDrawDesc region = host;
    switch (place) {
    case UiPlace::Fill:
        break;
    case UiPlace::Top: {
        const float height = host.size.y <= 0 ? 0 : std::min(slotHeight * widthScale, host.size.y);
        region.size = {host.size.x, height};
        break;
    }
    case UiPlace::Bottom: {
        const float height = host.size.y <= 0 ? 0 : std::min(slotHeight * widthScale, host.size.y);
        region.size = {host.size.x, height};
        region.position.y += host.size.y - height;
        break;
    }
    case UiPlace::Left: {
        const float width = host.size.x <= 0 ? 0 : std::min(slotWidth * heightScale, host.size.x);
        region.size = {width, host.size.y};
        break;
    }
    case UiPlace::Right: {
        const float width = host.size.x <= 0 ? 0 : std::min(slotWidth * heightScale, host.size.x);
        region.size = {width, host.size.y};
        region.position.x += host.size.x - width;
        break;
    }
    case UiPlace::Center:
    case UiPlace::Free: {
        const Vec2 size{host.size.x <= 0 ? 0 : std::min(slotWidth * fit, host.size.x),
                        host.size.y <= 0 ? 0 : std::min(slotHeight * fit, host.size.y)};
        region.size = size;
        region.position = host.position + (place == UiPlace::Free ? placePosition * fit : (host.size - size) * 0.5f);
        break;
    }
    }
    return region;
}

std::vector<UiElementLayout> UiDocument::ResolveLayout(const UiDrawDesc& desc) const
{
    Validate();
    if (!InRange(desc.position.x, -10000000, 10000000) || !InRange(desc.position.y, -10000000, 10000000) ||
        !InRange(desc.size.x, 0, 100000) || !InRange(desc.size.y, 0, 100000))
        throw std::runtime_error("UI draw rectangle is invalid");
    const UiDrawDesc region = Region(desc);
    const float scale = std::min(region.size.x / referenceSize.x, region.size.y / referenceSize.y);
    std::vector<UiElementLayout> result(elements.size());
    std::vector<bool> resolved(elements.size());
    std::unordered_map<std::string, usize> indices;
    for (usize index = 0; index < elements.size(); ++index) indices.emplace(elements[index].id, index);
    const UiElementLayout root{elements.size(), region.position, region.size, region.position, region.size, true, true};
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
        const Vec2 scaled = element.size * scale;
        switch (element.place) {
        case UiPlace::Fill:
            layout.position = parent->position;
            layout.size = parent->size;
            break;
        case UiPlace::Top:
            layout.size = {parent->size.x, std::min(scaled.y, parent->size.y)};
            layout.position = parent->position;
            break;
        case UiPlace::Bottom:
            layout.size = {parent->size.x, std::min(scaled.y, parent->size.y)};
            layout.position = {parent->position.x, parent->position.y + parent->size.y - layout.size.y};
            break;
        case UiPlace::Left:
            layout.size = {std::min(scaled.x, parent->size.x), parent->size.y};
            layout.position = parent->position;
            break;
        case UiPlace::Right:
            layout.size = {std::min(scaled.x, parent->size.x), parent->size.y};
            layout.position = {parent->position.x + parent->size.x - layout.size.x, parent->position.y};
            break;
        case UiPlace::Center:
            layout.size = {std::min(scaled.x, parent->size.x), std::min(scaled.y, parent->size.y)};
            layout.position = parent->position + (parent->size - layout.size) * 0.5f;
            break;
        case UiPlace::Free:
            layout.size = scaled;
            layout.position = parent->position + element.position * scale;
            layout.position.x += element.anchor.x * (parent->size.x - layout.size.x);
            layout.position.y += element.anchor.y * (parent->size.y - layout.size.y);
            break;
        }
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
