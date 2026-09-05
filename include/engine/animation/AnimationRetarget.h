// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONRETARGET_H
#define CONCORD_ANIMATIONRETARGET_H

#include "Concord/CExport.h"
#include "engine/animation/Humanoid.h"
#include "engine/asset/Animation.h"

namespace Concord {

/** How the source clip's hips translation survives retargeting. */
enum class RootMotionMode {
    /** Keep the hips translation as authored; playback moves the mesh itself. */
    KeepBaked,
    /** Drop horizontal hips motion and keep only the vertical component. */
    InPlace,
    /**
     * Strip horizontal motion from the body clip and emit a separate
     * hips-only clip carrying it, so game logic can drive the transform.
     */
    ExtractRootMotion,
};

/** Retargeting switches. */
struct RetargetOptions {
    RootMotionMode rootMotion = RootMotionMode::KeepBaked;
};

/** The retargeted body clip plus, optionally, its extracted root motion. */
struct CENGINE_API RetargetResult {
    /** Clip bound to the target skeleton's joint indices. */
    AnimationClip clip{};
    /** Whether `rootMotion` carries an extracted hips track. */
    bool hasRootMotion = false;
    /** Hips-only root-motion clip, populated in ExtractRootMotion mode. */
    AnimationClip rootMotion{};
};

/**
 * Maps a source clip onto a target humanoid rig by semantic bone slots.
 *
 * Channels resolve through the source's humanoid mapping and are rewritten
 * against the target's joints (direct joint indices, no source-node linkage).
 * Rotation and scale keys are copied verbatim; hips translation keys scale by
 * the ratio of bind-pose chain lengths so motion matches the target's size.
 * Channels on joints with no humanoid slot are dropped.
 *
 * The result clip is meant to be appended to the target asset's animation
 * list, after which graphs and components address it by clip index.
 */
[[nodiscard]] CENGINE_API bool RetargetClip(const HumanoidSkeleton& source,
                                            const AnimationClip& clip,
                                            const HumanoidSkeleton& target,
                                            const RetargetOptions& options,
                                            RetargetResult& out);

} // namespace Concord

#endif // CONCORD_ANIMATIONRETARGET_H
