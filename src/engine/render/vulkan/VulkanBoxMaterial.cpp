// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanBoxMaterial.h"

#include "engine/core/Color.h"
#include "engine/scene/Material.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Clamps into range with a caller-chosen default for a non-finite value. */
f32 SafeValue(f32 value, f32 fallback, f32 low, f32 high) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

} // namespace

VulkanBoxMaterial MakeVulkanBoxMaterial(const Material& material) noexcept
{
    const Vec3 albedo = ToLinear(material.albedo);
    VulkanBoxMaterial packed{};
    packed.albedo = {albedo.x, albedo.y, albedo.z,
                     static_cast<f32>(ColorA(material.albedo)) / 255.0f};
    // Emissive has no ceiling on purpose: it is an authoring choice, and the
    // tone mapper is what decides how far a bright surface is allowed to go.
    packed.surface = {SafeValue(material.metallic, 0.0f, 0.0f, 1.0f),
                      SafeValue(material.roughness, 0.8f, 0.04f, 1.0f),
                      std::isfinite(material.emissive) ? std::max(material.emissive, 0.0f) : 0.0f,
                      0.0f};
    return packed;
}

} // namespace Concord
