// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/GltfLoaderInternal.h"

#include <fstream>
#include <limits>

namespace Concord::AssetGltf {
namespace {

int Base64Digit(char value) noexcept
{
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    return value == '+' ? 62 : value == '/' ? 63 : -1;
}

bool DecodeDataUri(std::string_view uri, std::vector<std::byte>& output)
{
    const usize comma = uri.find(',');
    if (comma == std::string_view::npos || uri.substr(0, comma).find(";base64") == std::string_view::npos) return false;
    const std::string_view encoded = uri.substr(comma + 1);
    output.clear(); output.reserve(encoded.size() * 3 / 4);
    u32 accumulator = 0; u32 bits = 0;
    for (char value : encoded) {
        if (value == '=' || value == '\r' || value == '\n' || value == ' ' || value == '\t') continue;
        const int digit = Base64Digit(value);
        if (digit < 0) return false;
        accumulator = (accumulator << 6) | static_cast<u32>(digit); bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<std::byte>((accumulator >> bits) & 0xFFu));
        }
    }
    return true;
}

bool ReadBytes(const std::filesystem::path& path, std::vector<std::byte>& output)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize size = file.tellg();
    if (size < 0) return false;
    output.resize(static_cast<usize>(size)); file.seekg(0);
    return size == 0 || static_cast<bool>(file.read(reinterpret_cast<char*>(output.data()), size));
}

bool ReadSize(const AssetJson::Value* value, usize& result)
{
    return IndexValue(value, result) && result <= std::numeric_limits<usize>::max() - 1;
}

} // namespace

bool LoadBuffers(Context& context, std::span<const std::byte> glbBinary)
{
    const AssetJson::Value* buffers = Member(*context.root, "buffers");
    if (!buffers || !buffers->Is(AssetJson::Type::Array)) return context.Fail("glTF buffers array is missing");
    context.buffers.reserve(buffers->array.size());
    for (usize index = 0; index < buffers->array.size(); ++index) {
        const AssetJson::Value& record = buffers->array[index];
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF buffer record");
        usize declared = 0;
        if (!ReadSize(Member(record, "byteLength"), declared)) return context.Fail("invalid glTF buffer byteLength");
        std::vector<std::byte> bytes;
        const AssetJson::Value* uriValue = Member(record, "uri");
        if (uriValue && uriValue->Is(AssetJson::Type::String)) {
            const std::string_view uri = uriValue->String();
            if (uri.rfind("data:", 0) == 0) {
                if (!DecodeDataUri(uri, bytes)) return context.Fail("invalid glTF data URI");
            } else if (!ReadBytes(context.baseDirectory / std::string(uri), bytes)) {
                return context.Fail("unable to read glTF buffer URI");
            }
        } else if (index == 0 && !glbBinary.empty()) {
            bytes.assign(glbBinary.begin(), glbBinary.end());
        } else {
            return context.Fail("glTF buffer has no URI or GLB payload");
        }
        if (bytes.size() < declared) return context.Fail("glTF buffer is shorter than byteLength");
        bytes.resize(std::max(bytes.size(), declared));
        context.buffers.push_back(std::move(bytes));
    }
    return true;
}

bool ReadAccessorMetadata(Context& context)
{
    const AssetJson::Value* views = Member(*context.root, "bufferViews");
    if (views && views->Is(AssetJson::Type::Array)) {
        context.views.reserve(views->array.size());
        for (const AssetJson::Value& record : views->array) {
            if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF bufferView");
            BufferView view{};
            if (!IndexValue(Member(record, "buffer"), view.buffer) || view.buffer >= context.buffers.size() ||
                !ReadSize(Member(record, "byteLength"), view.length)) return context.Fail("invalid glTF bufferView range");
            if (Member(record, "byteOffset") && !ReadSize(Member(record, "byteOffset"), view.offset)) return context.Fail("invalid glTF bufferView offset");
            usize stride = 0;
            if (Member(record, "byteStride") && !ReadSize(Member(record, "byteStride"), stride)) return context.Fail("invalid glTF byteStride");
            view.stride = stride;
            if (view.offset > context.buffers[view.buffer].size() || view.length > context.buffers[view.buffer].size() - view.offset) {
                return context.Fail("glTF bufferView exceeds buffer");
            }
            context.views.push_back(view);
        }
    }
    const AssetJson::Value* accessors = Member(*context.root, "accessors");
    if (!accessors || !accessors->Is(AssetJson::Type::Array)) return context.Fail("glTF accessors array is missing");
    context.accessors.reserve(accessors->array.size());
    for (const AssetJson::Value& record : accessors->array) {
        if (!record.Is(AssetJson::Type::Object)) return context.Fail("invalid glTF accessor");
        Accessor accessor{};
        if (Member(record, "bufferView") && !SignedIndex(Member(record, "bufferView"), accessor.bufferView)) return context.Fail("invalid glTF accessor bufferView");
        if (Member(record, "byteOffset") && !ReadSize(Member(record, "byteOffset"), accessor.offset)) return context.Fail("invalid glTF accessor offset");
        usize componentType = 0;
        if (!ReadSize(Member(record, "count"), accessor.count) ||
            !IndexValue(Member(record, "componentType"), componentType) ||
            componentType > std::numeric_limits<u32>::max()) return context.Fail("invalid glTF accessor metadata");
        accessor.componentType = static_cast<u32>(componentType);
        accessor.normalized = Member(record, "normalized") ? Member(record, "normalized")->Bool() : false;
        accessor.type = std::string(Member(record, "type") ? Member(record, "type")->String() : std::string_view{});
        if (accessor.type != "SCALAR" && accessor.type != "VEC2" && accessor.type != "VEC3" && accessor.type != "VEC4" && accessor.type != "MAT4") return context.Fail("unsupported glTF accessor type");
        if (accessor.componentType != 5120 && accessor.componentType != 5121 && accessor.componentType != 5122 && accessor.componentType != 5123 && accessor.componentType != 5125 && accessor.componentType != 5126) return context.Fail("unsupported glTF component type");
        if (accessor.bufferView >= 0 && static_cast<usize>(accessor.bufferView) >= context.views.size()) return context.Fail("glTF accessor bufferView out of range");
        context.accessors.push_back(std::move(accessor));
    }
    return true;
}

} // namespace Concord::AssetGltf
