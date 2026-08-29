// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/ObjLoaderInternal.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Concord::AssetObj {
namespace {

ModelMaterial* FindMaterial(Builder& builder, std::string_view name)
{
    for (ModelMaterial& material : builder.asset.materials) {
        if (material.name == name) return &material;
    }
    builder.asset.materials.push_back(ModelMaterial{.name = std::string(name)});
    return &builder.asset.materials.back();
}

} // namespace

bool ParseMtlFile(Builder& builder, const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) return !builder.options.strict;
    ModelMaterial* current = nullptr;
    std::string raw;
    while (std::getline(file, raw)) {
        std::istringstream stream(raw);
        std::string keyword;
        stream >> keyword;
        if (keyword.empty() || keyword[0] == '#') continue;
        if (keyword == "newmtl") {
            std::string name; stream >> name; current = FindMaterial(builder, name);
        } else if (current && keyword == "Kd") {
            f32 r = 1.0f, g = 1.0f, b = 1.0f;
            if (stream >> r >> g >> b) {
                const auto ToByte = [](f32 value) { return static_cast<u8>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
                current->baseColor = MakeColor(ToByte(r), ToByte(g), ToByte(b));
            }
        } else if (current && keyword == "Ns") {
            f32 shininess = 0.0f;
            if (stream >> shininess) current->roughness = std::sqrt(2.0f / (shininess + 2.0f));
        } else if (current && keyword == "map_Kd") {
            std::string texture; stream >> texture; current->baseColorTexture = texture;
        }
    }
    return true;
}

} // namespace Concord::AssetObj
