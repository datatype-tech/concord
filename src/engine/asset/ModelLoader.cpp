// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ModelLoader.h"

#include "engine/asset/GltfLoaderInternal.h"
#include "engine/asset/ObjLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <cmath>

namespace Concord {
namespace {

std::string LowerExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

bool ReadFile(const std::filesystem::path& path, std::string& text)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize size = file.tellg();
    if (size < 0) return false;
    text.resize(static_cast<usize>(size)); file.seekg(0);
    return size == 0 || static_cast<bool>(file.read(text.data(), size));
}

bool ReadBinary(const std::filesystem::path& path, std::vector<std::byte>& bytes)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize size = file.tellg();
    if (size < 0) return false;
    bytes.resize(static_cast<usize>(size)); file.seekg(0);
    return size == 0 || static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
}

ModelLoadResult WithSourcePath(ModelLoadResult result, const std::filesystem::path& sourcePath)
{
    result.asset.sourcePath = sourcePath.lexically_normal();
    return result;
}

} // namespace

ModelLoadResult ModelLoader::Load(const std::filesystem::path& path,
                                  const ModelLoadOptions& options)
{
    if (!std::isfinite(options.scale)) { ModelLoadResult result; result.error.message = "model scale must be finite"; return result; }
    const std::string extension = LowerExtension(path);
    if (extension == ".obj") {
        std::string text;
        if (!ReadFile(path, text)) { ModelLoadResult result; result.error.message = "unable to read OBJ file"; return result; }
        return WithSourcePath(AssetObj::DecodeObj(text, options, path.parent_path()), path);
    }
    if (extension == ".gltf") {
        std::string text;
        if (!ReadFile(path, text)) { ModelLoadResult result; result.error.message = "unable to read glTF file"; return result; }
        return WithSourcePath(AssetGltf::DecodeDocument(text, path.parent_path(), {}, options), path);
    }
    if (extension == ".glb") {
        std::vector<std::byte> bytes;
        if (!ReadBinary(path, bytes)) { ModelLoadResult result; result.error.message = "unable to read GLB file"; return result; }
        return WithSourcePath(AssetGltf::DecodeGlb(bytes, path.parent_path(), options), path);
    }
    ModelLoadResult result; result.error.message = "unsupported model file extension"; return result;
}

ModelLoadResult ModelLoader::LoadObj(std::string_view text,
                                     const ModelLoadOptions& options)
{
    return LoadObj(text, std::filesystem::path{}, options);
}

ModelLoadResult ModelLoader::LoadObj(std::string_view text,
                                     const std::filesystem::path& baseDirectory,
                                     const ModelLoadOptions& options)
{
    ModelLoadResult result = AssetObj::DecodeObj(text, options, baseDirectory);
    result.asset.sourcePath = baseDirectory.lexically_normal();
    if (result.error.message.empty() && !result.asset.IsValid()) result.error.message = "OBJ contains no valid geometry";
    return result;
}

ModelLoadResult ModelLoader::LoadGlb(std::span<const std::byte> bytes,
                                     const std::filesystem::path& baseDirectory,
                                     const ModelLoadOptions& options)
{
    ModelLoadResult result = AssetGltf::DecodeGlb(bytes, baseDirectory, options);
    result.asset.sourcePath = baseDirectory.lexically_normal();
    return result;
}

ModelLoadResult ModelLoader::LoadGltf(std::string_view text,
                                      const std::filesystem::path& baseDirectory,
                                      const ModelLoadOptions& options)
{
    ModelLoadResult result = AssetGltf::DecodeDocument(text, baseDirectory, {}, options);
    result.asset.sourcePath = baseDirectory.lexically_normal();
    return result;
}

} // namespace Concord
