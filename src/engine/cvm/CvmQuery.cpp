// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/ParticleComponents.h"
#include "engine/physics/PhysicsQuery.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/ModelRenderer.h"
#include "engine/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

/**
 * Scene listing, naming and picking for the CVM C ABI.
 *
 * CVM cannot receive a list handle from the engine (those live in the script
 * runtime), so listing is a snapshot: one call fills a table, later calls
 * index it. Raycast stores the last hit so a script can read distance and
 * point without packing three values into one integer. When a PhysicsSystem
 * is running the ray is Jolt; otherwise it walks mesh AABBs.
 */
namespace {

std::vector<std::int64_t> g_snapshot;
double g_hitDistance = 0.0;
double g_hitX = 0.0;
double g_hitY = 0.0;
double g_hitZ = 0.0;

/** Parametric hit of a unit-length ray against an AABB, or false if missed. */
bool RayHitsAabb(const Concord::Vec3& origin, const Concord::Vec3& direction,
                 const Concord::Vec3& centre, const Concord::Vec3& half, float& hitTime)
{
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::infinity();
    const float originAxis[] = {origin.x, origin.y, origin.z};
    const float directionAxis[] = {direction.x, direction.y, direction.z};
    const float minimum[] = {centre.x - half.x, centre.y - half.y, centre.z - half.z};
    const float maximum[] = {centre.x + half.x, centre.y + half.y, centre.z + half.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(directionAxis[axis]) < 1.0e-8f) {
            if (originAxis[axis] < minimum[axis] || originAxis[axis] > maximum[axis]) {
                return false;
            }
            continue;
        }
        const float inverse = 1.0f / directionAxis[axis];
        float first = (minimum[axis] - originAxis[axis]) * inverse;
        float second = (maximum[axis] - originAxis[axis]) * inverse;
        if (first > second) {
            std::swap(first, second);
        }
        tMin = std::max(tMin, first);
        tMax = std::min(tMax, second);
        if (tMin > tMax) {
            return false;
        }
    }
    if (tMax < 0.0f) {
        return false;
    }
    hitTime = tMin >= 0.0f ? tMin : tMax;
    return hitTime >= 0.0f;
}

void ClearHit() noexcept
{
    g_hitDistance = 0.0;
    g_hitX = 0.0;
    g_hitY = 0.0;
    g_hitZ = 0.0;
}

} // namespace

extern "C" {

std::int64_t ConcordCvmSceneSnapshot(std::int64_t scene)
{
    g_snapshot.clear();
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    found->GetWorld().ForEachEntity([&](Concord::Entity entity) {
        g_snapshot.push_back(Concord::Cvm::FindOrCreateEntity(scene, *found, entity));
    });
    return static_cast<std::int64_t>(g_snapshot.size());
}

std::int64_t ConcordCvmSceneSnapshotAt(std::int64_t index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= g_snapshot.size()) return 0;
    return g_snapshot[static_cast<std::size_t>(index)];
}

std::int64_t ConcordCvmEntityKind(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    Concord::World& world = scene->GetWorld();
    if (world.Has<Concord::CameraComponent>(target)) return 1;
    if (world.Has<Concord::MeshRenderer>(target)) return 2;
    if (world.Has<Concord::ModelRenderer>(target)) return 3;
    if (world.Has<Concord::LightComponent>(target)) return 4;
    if (world.Has<Concord::ParticleEmitterComponent>(target)) return 5;
    return 0;
}

std::int64_t ConcordCvmEntitySetName(std::int64_t entity, const char* text)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    const char* value = text == nullptr ? "" : text;
    Concord::Name* name = scene->GetWorld().Get<Concord::Name>(target);
    if (name == nullptr) {
        scene->GetWorld().Add<Concord::Name>(target, Concord::Name{std::string(value)});
    } else {
        name->value = value;
    }
    return 1;
}

const char* ConcordCvmEntityName(std::int64_t entity)
{
    static const char kEmpty[] = "";
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return kEmpty;
    Concord::Name* name = scene->GetWorld().Get<Concord::Name>(target);
    return name == nullptr ? kEmpty : name->value.c_str();
}

std::int64_t ConcordCvmRaycast(std::int64_t scene, double originX, double originY, double originZ,
                               double directionX, double directionY, double directionZ,
                               double maxDistance)
{
    ClearHit();
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;
    if (!(maxDistance > 0.0) || !std::isfinite(maxDistance)) return 0;

    const double length =
        std::sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
    if (!(length > 1.0e-8)) return 0;
    const Concord::Vec3 origin = Concord::Cvm::ToVec3(originX, originY, originZ);
    const Concord::Vec3 direction =
        Concord::Cvm::ToVec3(directionX / length, directionY / length, directionZ / length);
    const float limit = static_cast<float>(maxDistance);

    if (Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*found)) {
        Concord::PhysicsRayHit physicsHit{};
        if (physics->Raycast(origin, direction, limit, physicsHit) &&
            physicsHit.entity.IsValid()) {
            g_hitDistance = static_cast<double>(physicsHit.distance);
            g_hitX = static_cast<double>(physicsHit.point.x);
            g_hitY = static_cast<double>(physicsHit.point.y);
            g_hitZ = static_cast<double>(physicsHit.point.z);
            return Concord::Cvm::FindOrCreateEntity(scene, *found, physicsHit.entity);
        }
    }

    Concord::Entity best{};
    float bestTime = limit;
    bool hit = false;
    found->GetWorld().Query<Concord::MeshRenderer, Concord::Transform>(
        [&](Concord::Entity entity, Concord::MeshRenderer&, Concord::Transform&) {
            Concord::Vec3 centre{};
            Concord::Vec3 half{};
            if (!Concord::Cvm::MeshBounds(*found, entity, centre, half)) return;
            float time = 0.0f;
            if (!RayHitsAabb(origin, direction, centre, half, time)) return;
            if (time > bestTime) return;
            bestTime = time;
            best = entity;
            hit = true;
        });
    if (!hit) return 0;

    g_hitDistance = static_cast<double>(bestTime);
    g_hitX = originX + (directionX / length) * g_hitDistance;
    g_hitY = originY + (directionY / length) * g_hitDistance;
    g_hitZ = originZ + (directionZ / length) * g_hitDistance;
    return Concord::Cvm::FindOrCreateEntity(scene, *found, best);
}

double ConcordCvmRaycastDistance(void)
{
    return g_hitDistance;
}

double ConcordCvmRaycastPoint(std::int64_t axis)
{
    if (axis == 0) return g_hitX;
    if (axis == 1) return g_hitY;
    if (axis == 2) return g_hitZ;
    return 0.0;
}

} // extern "C"
