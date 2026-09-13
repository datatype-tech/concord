// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"

#include <cmath>
#include <iostream>
#include <span>
#include <vector>

namespace {

using Concord::Mat4;
using Concord::ModelPrimitive;
using Concord::ModelVertex;
using Concord::SkinnedVertex;
using Concord::Vec3;

constexpr Concord::f32 kTolerance = 0.0005f;

bool Near(Vec3 a, Vec3 b)
{
    return std::abs(a.x - b.x) < kTolerance && std::abs(a.y - b.y) < kTolerance &&
           std::abs(a.z - b.z) < kTolerance;
}

/** Builds a primitive of `count` vertices sharing one joint/weight pattern. */
ModelPrimitive MakePrimitive(Concord::u32 count, Concord::u16 joint, Concord::f32 weight)
{
    ModelPrimitive primitive;
    for (Concord::u32 index = 0; index < count; ++index) {
        ModelVertex vertex;
        vertex.position = {static_cast<Concord::f32>(index), 1.0f, 2.0f};
        vertex.normal = {0.0f, 1.0f, 0.0f};
        vertex.joints = {joint, 0, 0, 0};
        vertex.weights = {weight, 0.0f, 0.0f, 0.0f};
        primitive.vertices.push_back(vertex);
    }
    primitive.indices = {0, 1, 2};
    return primitive;
}

bool Solve(const ModelPrimitive& primitive, std::span<const Mat4> joints,
           Concord::u32 firstJoint, Concord::u32 jointCount, std::vector<SkinnedVertex>& out)
{
    out.assign(primitive.vertices.size(), SkinnedVertex{});
    return Concord::SolveSkinnedVertices(primitive, joints, firstJoint, jointCount, out);
}

bool TestEmptyPaletteKeepsRestPose()
{
    const ModelPrimitive primitive = MakePrimitive(3, 0, 1.0f);
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, {}, 0, 0, out)) return false;
    return Near(out[2].position, {2.0f, 1.0f, 2.0f}) && Near(out[2].normal, {0.0f, 1.0f, 0.0f});
}

bool TestIdentityPaletteKeepsRestPose()
{
    const ModelPrimitive primitive = MakePrimitive(3, 0, 1.0f);
    const Mat4 joints[] = {Mat4::Identity()};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 1, out)) return false;
    return Near(out[1].position, {1.0f, 1.0f, 2.0f}) && Near(out[1].normal, {0.0f, 1.0f, 0.0f});
}

bool TestJointTranslationMovesPosition()
{
    const ModelPrimitive primitive = MakePrimitive(2, 0, 1.0f);
    const Mat4 joints[] = {Mat4::Translate({0.0f, 5.0f, -1.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 1, out)) return false;
    return Near(out[0].position, {0.0f, 6.0f, 1.0f}) && Near(out[0].normal, {0.0f, 1.0f, 0.0f});
}

bool TestBlendOfTwoJoints()
{
    const ModelPrimitive primitive = MakePrimitive(1, 0, 0.5f);
    ModelPrimitive blended = primitive;
    blended.vertices[0].joints = {0, 1, 0, 0};
    blended.vertices[0].weights = {0.5f, 0.5f, 0.0f, 0.0f};
    const Mat4 joints[] = {Mat4::Translate({0.0f, 0.0f, 0.0f}),
                           Mat4::Translate({0.0f, 2.0f, 0.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(blended, joints, 0, 2, out)) return false;
    return Near(out[0].position, {0.0f, 2.0f, 2.0f});
}

bool TestWeightsAreNormalized()
{
    const ModelPrimitive primitive = MakePrimitive(1, 0, 3.0f);
    ModelPrimitive scaled = primitive;
    scaled.vertices[0].weights = {3.0f, 0.0f, 0.0f, 0.0f};
    const Mat4 joints[] = {Mat4::Translate({1.0f, 0.0f, 0.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(scaled, joints, 0, 1, out)) return false;
    return Near(out[0].position, {1.0f, 1.0f, 2.0f});
}

bool TestUnusableWeightsKeepRestPose()
{
    const ModelPrimitive primitive = MakePrimitive(1, 0, 0.0f);
    const Mat4 joints[] = {Mat4::Translate({9.0f, 9.0f, 9.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 1, out)) return false;
    return Near(out[0].position, {0.0f, 1.0f, 2.0f});
}

bool TestJointIndexClampsIntoRange()
{
    const ModelPrimitive primitive = MakePrimitive(1, 9, 1.0f);
    const Mat4 joints[] = {Mat4::Identity(), Mat4::Translate({0.0f, 4.0f, 0.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 2, out)) return false;
    return Near(out[0].position, {0.0f, 5.0f, 2.0f});
}

bool TestFirstJointOffsetsThePalette()
{
    const ModelPrimitive primitive = MakePrimitive(1, 0, 1.0f);
    const Mat4 joints[] = {Mat4::Translate({9.0f, 0.0f, 0.0f}),
                           Mat4::Translate({0.0f, 0.0f, 7.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 1, 1, out)) return false;
    return Near(out[0].position, {0.0f, 1.0f, 9.0f});
}

bool TestNormalUsesInverseTranspose()
{
    const ModelPrimitive primitive = MakePrimitive(1, 0, 1.0f);
    const Mat4 joints[] = {Mat4::Scale({2.0f, 1.0f, 1.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 1, out)) return false;
    return Near(out[0].normal, {0.0f, 1.0f, 0.0f}) && Near(out[0].position, {0.0f, 1.0f, 2.0f});
}

bool TestShearShearsTheNormal()
{
    ModelPrimitive primitive = MakePrimitive(1, 0, 1.0f);
    primitive.vertices[0].normal = {1.0f, 1.0f, 0.0f};
    Mat4 shear = Mat4::Identity();
    shear.col[0].y = 1.0f;
    const Mat4 joints[] = {shear};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 1, out)) return false;
    // The shear maps (x, y, z) to (x, x + y, z), so its inverse transpose maps
    // the diagonal (1, 1, 0) onto the Y axis.
    return Near(out[0].normal, {0.0f, 1.0f, 0.0f});
}

bool TestUndersizedOutputFails()
{
    const ModelPrimitive primitive = MakePrimitive(4, 0, 1.0f);
    std::vector<SkinnedVertex> out(2);
    return !Concord::SolveSkinnedVertices(primitive, {}, 0, 0, out);
}

bool TestNonFiniteWeightIsDropped()
{
    ModelPrimitive primitive = MakePrimitive(1, 0, 1.0f);
    primitive.vertices[0].joints = {0, 1, 0, 0};
    primitive.vertices[0].weights = {1.0f, std::nanf(""), 0.0f, 0.0f};
    const Mat4 joints[] = {Mat4::Translate({0.0f, 1.0f, 0.0f}),
                           Mat4::Translate({0.0f, 50.0f, 0.0f})};
    std::vector<SkinnedVertex> out;
    if (!Solve(primitive, joints, 0, 2, out)) return false;
    return Near(out[0].position, {0.0f, 2.0f, 2.0f});
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"empty palette keeps rest pose", TestEmptyPaletteKeepsRestPose},
        {"identity palette keeps rest pose", TestIdentityPaletteKeepsRestPose},
        {"joint translation moves position", TestJointTranslationMovesPosition},
        {"two-joint blend", TestBlendOfTwoJoints},
        {"weights are normalized", TestWeightsAreNormalized},
        {"unusable weights keep rest pose", TestUnusableWeightsKeepRestPose},
        {"joint index clamps into range", TestJointIndexClampsIntoRange},
        {"firstJoint offsets the palette", TestFirstJointOffsetsThePalette},
        {"normal uses the inverse transpose", TestNormalUsesInverseTranspose},
        {"shear shears the normal", TestShearShearsTheNormal},
        {"undersized output fails", TestUndersizedOutputFails},
        {"non-finite weight is dropped", TestNonFiniteWeightIsDropped},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    std::cout << (sizeof(cases) / sizeof(cases[0]) - failures) << '/'
              << (sizeof(cases) / sizeof(cases[0])) << " skinned geometry cases passed\n";
    return failures == 0 ? 0 : 1;
}
