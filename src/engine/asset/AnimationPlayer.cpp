// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/Animation.h"

#include <cmath>

namespace Concord {

void AnimationPlayer::Play(const AnimationClip* clip, bool loop) noexcept
{
    m_clip = clip;
    m_loop = loop;
    m_time = 0.0f;
    m_playing = clip != nullptr;
}

bool AnimationPlayer::Update(const Skeleton& skeleton, f32 deltaSeconds,
                             SkeletonPose& pose) noexcept
{
    if (!m_clip) {
        pose.Reset(skeleton);
        return false;
    }
    if (!m_playing) return true;
    if (std::isfinite(deltaSeconds)) {
        m_time += deltaSeconds;
    }
    if (m_clip->duration > 0.0f && !m_loop && m_time >= m_clip->duration) {
        m_time = m_clip->duration;
        m_playing = false;
    }
    return SampleAnimation(skeleton, *m_clip, m_time, pose, m_loop);
}

} // namespace Concord
