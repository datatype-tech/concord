// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_MODELASSET_H
#define CONCORD_ASSET_MODELASSET_H

#include "Concord/CExport.h"
#include "engine/asset/Animation.h"
#include "engine/core/Color.h"
#include "engine/core/Vec2.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace Concord {

/** Vertex layout shared by static and skinned imported primitives. */
struct ModelVertex {
    Vec3 position{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    Vec2 texcoord{};
    std::array<u16, 4> joints{};
    Vec4 weights{1.0f, 0.0f, 0.0f, 0.0f};
};

/** One material as imported from glTF or a lightweight MTL file. */
struct ModelMaterial {
    std::string name;
    ColorRGBA baseColor = COLOR_RGB(200, 200, 200);
    f32 metallic = 0.0f;
    f32 roughness = 0.8f;
    Vec3 emissive{};
    std::string baseColorTexture;
};

/** Indexed triangle geometry and its material assignment. */
struct ModelPrimitive {
    std::vector<ModelVertex> vertices;
    std::vector<u32> indices;
    u32 materialIndex = 0;
    u32 mode = 4;
};

/** A named mesh containing one or more primitives. */
struct ModelMesh {
    std::string name;
    std::vector<ModelPrimitive> primitives;
};

/** A scene node; children refer to indices in ModelAsset::nodes. */
struct ModelNode {
    std::string name;
    i32 parent = -1;
    std::vector<u32> children;
    BoneTransform local{};
    i32 mesh = -1;
    i32 skin = -1;
};

/** Fully decoded CPU-side model, including optional skins and animations. */
struct CENGINE_API ModelAsset {
    std::string name;
    std::filesystem::path sourcePath;
    std::vector<ModelMesh> meshes;
    std::vector<ModelMaterial> materials;
    std::vector<ModelNode> nodes;
    std::vector<Skeleton> skeletons;
    std::vector<AnimationClip> animations;

    /** Returns false for an empty or structurally inconsistent asset. */
    [[nodiscard]] bool IsValid() const noexcept;
    /** Finds an animation by exact name, or returns nullptr. */
    [[nodiscard]] const AnimationClip* FindAnimation(const std::string& clipName) const noexcept;
};

} // namespace Concord

#endif // CONCORD_ASSET_MODELASSET_H
