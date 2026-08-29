// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_MODELLOADER_H
#define CONCORD_ASSET_MODELLOADER_H

#include "Concord/CExport.h"
#include "engine/asset/ModelAsset.h"

#include <filesystem>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace Concord {

/** Options shared by OBJ, glTF and GLB import. */
struct ModelLoadOptions {
    bool generateNormals = true;
    bool generateTangents = true;
    bool flipV = false;
    bool strict = true;
    f32 scale = 1.0f;
};

/** Stable diagnostic returned when decoding fails. */
struct ModelLoadError {
    std::string message;
    usize line = 0;
    usize column = 0;
};

/** Value type returned by every loader entry point. */
struct ModelLoadResult {
    ModelAsset asset;
    ModelLoadError error;

    /** Whether a complete, validated asset was produced. */
    [[nodiscard]] bool Succeeded() const noexcept { return error.message.empty() && asset.IsValid(); }
    /** Convenience access to the diagnostic text, empty on success. */
    [[nodiscard]] const std::string& ErrorMessage() const noexcept { return error.message; }
};

/** Decodes the supported interchange formats into a deterministic CPU asset. */
class CENGINE_API ModelLoader {
public:
    /** Loads OBJ, glTF JSON or binary GLB based on the file extension. */
    [[nodiscard]] static ModelLoadResult Load(const std::filesystem::path& path,
                                              const ModelLoadOptions& options = {});
    /** Loads OBJ text; useful for generated assets and unit tests. */
    [[nodiscard]] static ModelLoadResult LoadObj(std::string_view text,
                                                 const ModelLoadOptions& options = {});
    /** Loads OBJ text while resolving an optional relative MTL directory. */
    [[nodiscard]] static ModelLoadResult LoadObj(std::string_view text,
                                                 const std::filesystem::path& baseDirectory,
                                                 const ModelLoadOptions& options = {});
    /** Loads glTF JSON text and resolves buffers relative to `baseDirectory`. */
    [[nodiscard]] static ModelLoadResult LoadGltf(std::string_view text,
                                                  const std::filesystem::path& baseDirectory = {},
                                                  const ModelLoadOptions& options = {});
    /** Loads an in-memory GLB container without touching the filesystem. */
    [[nodiscard]] static ModelLoadResult LoadGlb(std::span<const std::byte> bytes,
                                                 const std::filesystem::path& baseDirectory = {},
                                                 const ModelLoadOptions& options = {});
};

} // namespace Concord

#endif // CONCORD_ASSET_MODELLOADER_H
