// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"
#include "Concord/CRender.h"

#include <cstddef>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

bool IsIdentity(const Concord::Mat4& matrix)
{
    const Concord::Mat4 identity = Concord::Mat4::Identity();
    for (Concord::u32 column = 0; column < 4; ++column) {
        for (Concord::u32 row = 0; row < 4; ++row) {
            if (std::abs(matrix.col[column][row] - identity.col[column][row]) > 0.0001f) {
                return false;
            }
        }
    }
    return true;
}

bool TestRangeAndBytes()
{
    Concord::SkinningPaletteUpload upload;
    const Concord::Mat4 source[] = {Concord::Mat4::Translate({1.0f, 2.0f, 3.0f}),
                                    Concord::Mat4::Scale({2.0f, 2.0f, 2.0f})};
    const auto range = Concord::AppendSkinningPalette(upload, source);
    return range.firstJoint == 0 && range.jointCount == 2 && range.flags == 0 &&
           Concord::SkinningPaletteBytes(upload).size() == 128;
}

bool TestMalformedAndBoundedInput()
{
    Concord::SkinningPaletteUpload upload;
    Concord::Mat4 malformed = Concord::Mat4::Identity();
    malformed.col[2].x = std::numeric_limits<Concord::f32>::quiet_NaN();
    const auto first = Concord::AppendSkinningPalette(
        upload, std::span<const Concord::Mat4>(&malformed, 1));
    if (first.jointCount != 1 || !IsIdentity(upload.jointMatrices.front())) return false;
    std::vector<Concord::Mat4> source(Concord::kMaxSkinningJoints + 3,
                                      Concord::Mat4::Identity());
    const auto second = Concord::AppendSkinningPalette(upload, source);
    return second.firstJoint == 1 && second.jointCount == Concord::kMaxSkinningJoints &&
           (second.flags & Concord::kSkinningPaletteTruncated) != 0;
}

bool TestAnimationComponentDefaults()
{
    const Concord::AnimationComponent animation;
    const Concord::SkinningPoseComponent pose;
    return animation.asset == nullptr && animation.skeletonIndex == Concord::kInvalidAnimationIndex &&
           animation.loop && animation.playing && animation.speed == 1.0f &&
           pose.skeletonIndex == Concord::kInvalidAnimationIndex && pose.local.empty() &&
           pose.jointMatrices.empty();
}

bool TestVertexLayout()
{
    return Concord::kSkinningVertexStride == sizeof(Concord::ModelVertex) &&
           Concord::kSkinningPositionOffset == offsetof(Concord::ModelVertex, position) &&
           Concord::kSkinningNormalOffset == offsetof(Concord::ModelVertex, normal) &&
           Concord::kSkinningJointsOffset == offsetof(Concord::ModelVertex, joints) &&
           Concord::kSkinningWeightsOffset == offsetof(Concord::ModelVertex, weights);
}

} // namespace

int main()
{
    const bool passed = TestRangeAndBytes() && TestMalformedAndBoundedInput() &&
                        TestAnimationComponentDefaults() && TestVertexLayout();
    if (!passed) std::cerr << "Skinning palette regression failed\n";
    return passed ? 0 : 1;
}
