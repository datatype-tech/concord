// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Concord::AssetGltf {
namespace {

u32 ComponentCount(std::string_view type) noexcept
{
    return type == "SCALAR" ? 1u : type == "VEC2" ? 2u : type == "VEC3" ? 3u :
           type == "VEC4" ? 4u : type == "MAT4" ? 16u : 0u;
}

usize ComponentSize(u32 type) noexcept
{
    return type == 5126 || type == 5125 ? 4u : type == 5122 || type == 5123 ? 2u : 1u;
}

bool ElementSpan(const Context& context, const Accessor& accessor, usize element,
                 u32 components, const std::byte*& pointer, usize& stride) noexcept
{
    if (accessor.bufferView < 0) return false;
    const BufferView& view = context.views[static_cast<usize>(accessor.bufferView)];
    const usize size = ComponentSize(accessor.componentType) * components;
    stride = view.stride == 0 ? size : view.stride;
    if (stride < size || element > (std::numeric_limits<usize>::max() - accessor.offset) / stride) return false;
    const usize relative = accessor.offset + element * stride;
    if (relative > view.length || size > view.length - relative) return false;
    pointer = context.buffers[view.buffer].data() + view.offset + relative;
    return true;
}

f64 ReadScalar(const std::byte* pointer, u32 type, bool normalized) noexcept
{
    switch (type) {
    case 5120: { i8 value{}; std::memcpy(&value, pointer, 1); return normalized ? std::max(-1.0, static_cast<f64>(value) / 128.0) : value; }
    case 5121: { u8 value{}; std::memcpy(&value, pointer, 1); return normalized ? static_cast<f64>(value) / 255.0 : value; }
    case 5122: { i16 value{}; std::memcpy(&value, pointer, 2); return normalized ? std::max(-1.0, static_cast<f64>(value) / 32768.0) : value; }
    case 5123: { u16 value{}; std::memcpy(&value, pointer, 2); return normalized ? static_cast<f64>(value) / 65535.0 : value; }
    case 5125: { u32 value{}; std::memcpy(&value, pointer, 4); return normalized ? static_cast<f64>(value) / 4294967295.0 : value; }
    case 5126: { f32 value{}; std::memcpy(&value, pointer, 4); return std::isfinite(value) ? value : 0.0; }
    default: return 0.0;
    }
}

bool GetAccessor(const Context& context, i32 index, const Accessor*& accessor) noexcept
{
    if (index < 0 || static_cast<usize>(index) >= context.accessors.size()) return false;
    accessor = &context.accessors[static_cast<usize>(index)];
    return true;
}

} // namespace

bool ReadFloatAccessor(const Context& context, i32 accessorIndex, u32 components,
                       std::vector<f32>& values)
{
    const Accessor* accessor = nullptr;
    if (!GetAccessor(context, accessorIndex, accessor) || ComponentCount(accessor->type) != components ||
        accessor->count > std::numeric_limits<usize>::max() / components) return false;
    values.assign(accessor->count * components, 0.0f);
    for (usize element = 0; element < accessor->count; ++element) {
        const std::byte* pointer = nullptr; usize stride = 0;
        if (!ElementSpan(context, *accessor, element, components, pointer, stride)) return false;
        for (u32 component = 0; component < components; ++component) {
            const f64 value = ReadScalar(pointer + component * ComponentSize(accessor->componentType), accessor->componentType, accessor->normalized);
            values[element * components + component] = std::isfinite(value) ? static_cast<f32>(value) : 0.0f;
        }
    }
    return true;
}

bool ReadIndexAccessor(const Context& context, i32 accessorIndex,
                       std::vector<u32>& values)
{
    const Accessor* accessor = nullptr;
    if (!GetAccessor(context, accessorIndex, accessor) || accessor->type != "SCALAR" ||
        (accessor->componentType != 5121 && accessor->componentType != 5123 &&
         accessor->componentType != 5125)) return false;
    values.assign(accessor->count, 0u);
    for (usize element = 0; element < accessor->count; ++element) {
        const std::byte* pointer = nullptr; usize stride = 0;
        if (!ElementSpan(context, *accessor, element, 1, pointer, stride)) return false;
        const f64 value = ReadScalar(pointer, accessor->componentType, false);
        if (value < 0.0 || value > static_cast<f64>(std::numeric_limits<u32>::max())) return false;
        values[element] = static_cast<u32>(value);
    }
    return true;
}

bool ReadJointAccessor(const Context& context, i32 accessorIndex,
                       std::vector<std::array<u16, 4>>& values)
{
    const Accessor* accessor = nullptr;
    if (!GetAccessor(context, accessorIndex, accessor) || ComponentCount(accessor->type) != 4 ||
        accessor->normalized ||
        (accessor->componentType != 5121 && accessor->componentType != 5123 && accessor->componentType != 5125)) return false;
    std::vector<f32> decoded;
    if (!ReadFloatAccessor(context, accessorIndex, 4, decoded)) return false;
    values.resize(accessor->count);
    for (usize i = 0; i < accessor->count; ++i) {
        for (u32 j = 0; j < 4; ++j) values[i][j] = static_cast<u16>(std::clamp(decoded[i * 4 + j], 0.0f, 65535.0f));
    }
    return true;
}

} // namespace Concord::AssetGltf
