// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_OBJLOADERINTERNAL_H
#define CONCORD_ASSET_OBJLOADERINTERNAL_H

#include "engine/asset/ObjLoader.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Concord::AssetObj {

struct Index {
    i32 position = 0;
    i32 texcoord = 0;
    i32 normal = 0;
    friend bool operator==(Index, Index) noexcept = default;
};

struct IndexHash {
    usize operator()(Index value) const noexcept
    {
        const usize a = static_cast<usize>(static_cast<u32>(value.position));
        const usize b = static_cast<usize>(static_cast<u32>(value.texcoord));
        const usize c = static_cast<usize>(static_cast<u32>(value.normal));
        return (a * 73856093u) ^ (b * 19349663u) ^ (c * 83492791u);
    }
};

struct Builder {
    ModelAsset asset;
    ModelPrimitive primitive;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;
    std::vector<bool> hasNormal;
    std::unordered_map<Index, u32, IndexHash> vertices;
    u32 materialIndex = 0;
    usize meshIndex = 0;
    ModelLoadOptions options{};
    usize line = 0;
    bool failed = false;
    std::string error;
};

void FinishPrimitive(Builder& builder);
bool ParseFace(Builder& builder, std::string_view text);
bool ParseMtlFile(Builder& builder, const std::filesystem::path& path);
void GenerateNormals(Builder& builder);
void GenerateTangents(Builder& builder);

} // namespace Concord::AssetObj

#endif // CONCORD_ASSET_OBJLOADERINTERNAL_H
