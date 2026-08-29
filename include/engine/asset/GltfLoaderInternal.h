// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_GLTFLOADERINTERNAL_H
#define CONCORD_ASSET_GLTFLOADERINTERNAL_H

#include "engine/asset/JsonValue.h"
#include "engine/asset/ModelLoader.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Concord::AssetGltf {

/** One glTF buffer view after URI or GLB resolution. */
struct BufferView {
    usize buffer = 0;
    usize offset = 0;
    usize length = 0;
    usize stride = 0;
};

/** Metadata needed to decode one glTF accessor. */
struct Accessor {
    i32 bufferView = -1;
    usize offset = 0;
    usize count = 0;
    u32 componentType = 0;
    bool normalized = false;
    std::string type;
};

/** Shared state passed between the split glTF decoding translation units. */
struct Context {
    const AssetJson::Value* root = nullptr;
    std::filesystem::path baseDirectory;
    ModelLoadOptions options{};
    std::vector<std::vector<std::byte>> buffers;
    std::vector<BufferView> views;
    std::vector<Accessor> accessors;
    ModelAsset asset;
    ModelLoadError error;

    /** Records the first decoding error and its source location. */
    bool Fail(std::string message, usize line = 0, usize column = 0) noexcept;
};

/** Returns an object member, or nullptr when the value is not an object. */
const AssetJson::Value* Member(const AssetJson::Value& value,
                               std::string_view key) noexcept;

/** Converts a JSON number to a bounded array index. */
bool IndexValue(const AssetJson::Value* value, usize& result) noexcept;

/** Converts a JSON number to a signed index, preserving -1 sentinels. */
bool SignedIndex(const AssetJson::Value* value, i32& result) noexcept;

/** Reads a finite JSON number as a float. */
bool FloatValue(const AssetJson::Value* value, f32& result) noexcept;

/** Loads external/data URI buffers referenced by a glTF document. */
bool LoadBuffers(Context& context, std::span<const std::byte> glbBinary);

/** Builds buffer-view and accessor metadata from the JSON document. */
bool ReadAccessorMetadata(Context& context);

/** Reads a floating-point accessor with the requested component count. */
bool ReadFloatAccessor(const Context& context, i32 accessorIndex, u32 components,
                       std::vector<f32>& values);

/** Reads an unsigned integer index accessor. */
bool ReadIndexAccessor(const Context& context, i32 accessorIndex,
                       std::vector<u32>& values);

/** Reads a joint-index accessor into four-component vertex weights. */
bool ReadJointAccessor(const Context& context, i32 accessorIndex,
                       std::vector<std::array<u16, 4>>& values);

/** Decodes meshes and PBR materials into the model asset. */
bool ReadGeometry(Context& context);

/** Decodes glTF material records and image references. */
bool ReadMaterials(Context& context);

/** Decodes mesh primitives and generates missing tangent-space data. */
bool ReadMeshes(Context& context);

/** Decodes nodes, skins and animation clips into the model asset. */
bool ReadScene(Context& context);

/** Decodes the node hierarchy and local transforms. */
bool ReadNodes(Context& context);

/** Decodes skin joint lists and inverse bind matrices. */
bool ReadSkins(Context& context);

/** Decodes animation samplers and channel keyframes. */
bool ReadAnimations(Context& context);

/** Parses a JSON glTF document, optionally with a GLB BIN chunk. */
ModelLoadResult DecodeDocument(std::string_view text,
                               const std::filesystem::path& baseDirectory,
                               std::span<const std::byte> glbBinary,
                               const ModelLoadOptions& options);

/** Parses a binary GLB container and forwards its JSON/BIN chunks. */
ModelLoadResult DecodeGlb(std::span<const std::byte> bytes,
                          const std::filesystem::path& baseDirectory,
                          const ModelLoadOptions& options);

} // namespace Concord::AssetGltf

#endif // CONCORD_ASSET_GLTFLOADERINTERNAL_H
