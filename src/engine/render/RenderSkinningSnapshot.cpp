// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderSkinningSnapshot.h"

#include "engine/ecs/AnimationComponents.h"

namespace Concord {
namespace {

void TagEntity(RenderSceneSnapshot& snapshot, Entity entity,
               const AnimationComponent& animation, SkinningPaletteRange range)
{
    for (RenderObjectSnapshot& object : snapshot.objects) {
        if (object.entity != entity || object.modelSkin < 0 ||
            object.modelAsset.get() != animation.asset ||
            object.modelSkin != static_cast<i32>(animation.skeletonIndex)) {
            continue;
        }
        object.skinningRange = range;
    }
}

bool HasMatchingModel(const RenderSceneSnapshot& snapshot, Entity entity,
                      const AnimationComponent& animation) noexcept
{
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.entity == entity && object.modelSkin >= 0 &&
            object.modelAsset.get() == animation.asset &&
            object.modelSkin == static_cast<i32>(animation.skeletonIndex)) {
            return true;
        }
    }
    return false;
}

} // namespace

void AppendSkinningSnapshots(RenderSceneSnapshot& snapshot, const World& world)
{
    world.Query<AnimationComponent, SkinningPoseComponent>(
        [&](Entity entity, const AnimationComponent& animation,
            const SkinningPoseComponent& pose) {
            if (animation.asset == nullptr || pose.sourceAsset != animation.asset ||
                pose.skeletonIndex != animation.skeletonIndex || pose.jointMatrices.empty()) {
                return;
            }
            if (animation.skeletonIndex >= animation.asset->skeletons.size() ||
                !animation.asset->skeletons[animation.skeletonIndex].IsValid() ||
                pose.jointMatrices.size() !=
                    animation.asset->skeletons[animation.skeletonIndex].joints.size() ||
                !HasMatchingModel(snapshot, entity, animation)) {
                return;
            }
            const usize previousSize = snapshot.skinningPalette.jointMatrices.size();
            const SkinningPaletteRange range =
                AppendSkinningPalette(snapshot.skinningPalette, pose.jointMatrices);
            if (range.jointCount == 0 || range.flags != 0) {
                snapshot.skinningPalette.jointMatrices.resize(previousSize);
                return;
            }
            TagEntity(snapshot, entity, animation, range);
        });
}

} // namespace Concord
