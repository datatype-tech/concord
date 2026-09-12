// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/physics/PhysicsWorld.h"

#include "engine/core/Angle.h"
#include "engine/core/Transform.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/World.h"
#include "engine/scene/Scene.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Concord {
namespace {

using JPH::BroadPhaseLayer;
using JPH::ObjectLayer;

constexpr ObjectLayer kLayerNonMoving = 0;
constexpr ObjectLayer kLayerMoving = 1;
constexpr unsigned kBroadPhaseNonMoving = 0;
constexpr unsigned kBroadPhaseMoving = 1;

std::mutex g_joltMutex;
int g_joltUsers = 0;

std::mutex g_bindMutex;
std::unordered_map<const Scene*, PhysicsWorld*> g_bound;

void TraceImpl(const char* format, ...)
{
    (void)format;
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char*, const char*, const char*, unsigned)
{
    return true;
}
#endif

void AcquireJolt()
{
    std::lock_guard<std::mutex> guard(g_joltMutex);
    if (g_joltUsers++ > 0) {
        return;
    }
    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

void ReleaseJolt()
{
    std::lock_guard<std::mutex> guard(g_joltMutex);
    if (--g_joltUsers > 0) {
        return;
    }
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayers()
    {
        m_layers[kLayerNonMoving] = BroadPhaseLayer(kBroadPhaseNonMoving);
        m_layers[kLayerMoving] = BroadPhaseLayer(kBroadPhaseMoving);
    }

    unsigned int GetNumBroadPhaseLayers() const override { return 2; }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override
    {
        return layer == kLayerMoving ? m_layers[kLayerMoving] : m_layers[kLayerNonMoving];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override
    {
        return layer == BroadPhaseLayer(kBroadPhaseMoving) ? "Moving" : "NonMoving";
    }
#endif

private:
    BroadPhaseLayer m_layers[2]{};
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer layer, BroadPhaseLayer broad) const override
    {
        if (layer == kLayerNonMoving) {
            return broad == BroadPhaseLayer(kBroadPhaseMoving);
        }
        return true;
    }
};

class ObjectLayerPair final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer left, ObjectLayer right) const override
    {
        if (left == kLayerNonMoving && right == kLayerNonMoving) {
            return false;
        }
        return true;
    }
};

u64 PackEntity(Entity entity) noexcept
{
    return (static_cast<u64>(entity.generation) << 32) | static_cast<u64>(entity.index);
}

Entity UnpackEntity(u64 packed, u64 worldId) noexcept
{
    Entity entity{};
    entity.index = static_cast<u32>(packed & 0xFFFFFFFFu);
    entity.generation = static_cast<u32>(packed >> 32);
    entity.worldId = worldId;
    return entity;
}

JPH::Vec3 ToJolt(Vec3 value) noexcept
{
    return {value.x, value.y, value.z};
}

Vec3 FromJolt(JPH::Vec3Arg value) noexcept
{
    return {value.GetX(), value.GetY(), value.GetZ()};
}

/** Concord rotation is Ry * Rx * Rz, the same product Transform::ToMatrix uses. */
JPH::Quat QuatFromTransform(const Transform& transform) noexcept
{
    const JPH::Quat yaw = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), Radians(transform.rotation.y));
    const JPH::Quat pitch = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), Radians(transform.rotation.x));
    const JPH::Quat roll = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), Radians(transform.rotation.z));
    return yaw * pitch * roll;
}

Vec3 EulerFromQuat(JPH::QuatArg rotation) noexcept
{
    const JPH::Mat44 matrix = JPH::Mat44::sRotation(rotation);
    const JPH::Vec3 axisZ = matrix.GetAxisZ();
    const f32 pitch = std::asin(std::clamp(axisZ.GetY(), -1.0f, 1.0f));
    const f32 cosine = std::cos(pitch);
    f32 yaw = 0.0f;
    f32 roll = 0.0f;
    if (std::fabs(cosine) > 1.0e-5f) {
        yaw = std::atan2(-axisZ.GetX(), axisZ.GetZ());
        const JPH::Vec3 axisX = matrix.GetAxisX();
        const JPH::Vec3 axisY = matrix.GetAxisY();
        roll = std::atan2(axisX.GetY(), axisY.GetY());
    } else {
        yaw = std::atan2(matrix.GetAxisY().GetX(), matrix.GetAxisX().GetX());
    }
    return {Degrees(pitch), Degrees(yaw), Degrees(roll)};
}

Vec3 AbsScale(const Transform& transform) noexcept
{
    return {std::fabs(transform.scale.x) > 1.0e-5f ? std::fabs(transform.scale.x) : 1.0f,
            std::fabs(transform.scale.y) > 1.0e-5f ? std::fabs(transform.scale.y) : 1.0f,
            std::fabs(transform.scale.z) > 1.0e-5f ? std::fabs(transform.scale.z) : 1.0f};
}

u64 Fingerprint(const Collider& collider, const RigidBody* body, const Transform& transform) noexcept
{
    const Vec3 scale = AbsScale(transform);
    const auto HashBits = [](f32 value) {
        u32 bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return static_cast<u64>(bits);
    };
    u64 hash = static_cast<u64>(collider.shape) + 1;
    hash = hash * 16777619u ^ HashBits(collider.size.x * scale.x);
    hash = hash * 16777619u ^ HashBits(collider.size.y * scale.y);
    hash = hash * 16777619u ^ HashBits(collider.size.z * scale.z);
    hash = hash * 16777619u ^ HashBits(collider.offset.x);
    hash = hash * 16777619u ^ HashBits(collider.offset.y);
    hash = hash * 16777619u ^ HashBits(collider.offset.z);
    hash = hash * 16777619u ^ (collider.isTrigger ? 1u : 0u);
    if (body != nullptr) {
        hash = hash * 16777619u ^ static_cast<u64>(body->motion);
        hash = hash * 16777619u ^ HashBits(body->mass);
        hash = hash * 16777619u ^ HashBits(body->friction);
        hash = hash * 16777619u ^ HashBits(body->restitution);
        hash = hash * 16777619u ^ (body->lockRotation ? 1u : 0u);
    }
    return hash;
}

JPH::RefConst<JPH::Shape> MakeShape(const Collider& collider, const Transform& transform)
{
    const Vec3 scale = AbsScale(transform);
    const Vec3 extent{std::max(collider.size.x * scale.x, 0.02f),
                      std::max(collider.size.y * scale.y, 0.02f),
                      std::max(collider.size.z * scale.z, 0.02f)};
    JPH::RefConst<JPH::Shape> shape;
    switch (collider.shape) {
    case CollisionShape::Sphere:
        shape = new JPH::SphereShape(std::max(extent.x * 0.5f, 0.01f));
        break;
    case CollisionShape::Capsule: {
        const f32 radius = std::max(extent.x * 0.5f, 0.01f);
        const f32 halfCylinder = std::max(extent.y * 0.5f - radius, 0.01f);
        shape = new JPH::CapsuleShape(halfCylinder, radius);
        break;
    }
    case CollisionShape::Box:
    default: {
        const JPH::Vec3 half = ToJolt(extent * 0.5f);
        const f32 convex =
            std::min(0.02f, std::min(half.GetX(), std::min(half.GetY(), half.GetZ())) * 0.4f);
        shape = new JPH::BoxShape(half, std::max(convex, 0.001f));
        break;
    }
    }
    if (std::fabs(collider.offset.x) + std::fabs(collider.offset.y) +
            std::fabs(collider.offset.z) >
        1.0e-5f) {
        shape = new JPH::RotatedTranslatedShape(ToJolt(collider.offset), JPH::Quat::sIdentity(),
                                                shape);
    }
    return shape;
}

JPH::EMotionType MotionTypeOf(const RigidBody* body) noexcept
{
    if (body == nullptr) {
        return JPH::EMotionType::Static;
    }
    switch (body->motion) {
    case BodyMotion::Kinematic:
        return JPH::EMotionType::Kinematic;
    case BodyMotion::Dynamic:
        return JPH::EMotionType::Dynamic;
    case BodyMotion::Static:
    default:
        return JPH::EMotionType::Static;
    }
}

} // namespace

struct PhysicsWorld::Impl {
    PhysicsSettings settings{};
    BroadPhaseLayers broadPhaseLayers{};
    ObjectVsBroadPhase objectVsBroadPhase{};
    ObjectLayerPair objectLayerPair{};
    JPH::TempAllocatorImpl allocator{8 * 1024 * 1024};
    JPH::JobSystemThreadPool jobs{
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
        std::thread::hardware_concurrency() > 1
            ? static_cast<int>(std::thread::hardware_concurrency() - 1)
            : 1};
    JPH::PhysicsSystem physics{};
    std::unordered_map<u64, JPH::BodyID> bodies{};
    std::unordered_map<u64, u64> fingerprints{};
    std::unordered_map<u64, JPH::Ref<JPH::CharacterVirtual>> characters{};
    Scene* scene = nullptr;
    f32 accumulator = 0.0f;

    Impl(const PhysicsSettings& authored) : settings(authored)
    {
        if (!std::isfinite(settings.fixedDeltaSeconds) || settings.fixedDeltaSeconds < 0.0f) {
            settings.fixedDeltaSeconds = 1.0f / 60.0f;
        }
        if (settings.collisionSteps == 0) {
            settings.collisionSteps = 1;
        }
        physics.Init(2048, 0, 4096, 4096, broadPhaseLayers, objectVsBroadPhase, objectLayerPair);
        physics.SetGravity(ToJolt(settings.gravity));
    }

    void DestroyBody(u64 key)
    {
        const auto found = bodies.find(key);
        if (found == bodies.end()) {
            return;
        }
        JPH::BodyInterface& interface = physics.GetBodyInterface();
        interface.RemoveBody(found->second);
        interface.DestroyBody(found->second);
        bodies.erase(found);
        fingerprints.erase(key);
    }

    void SyncBodies(Scene& scene)
    {
        World& world = scene.GetWorld();
        std::unordered_set<u64> live;
        JPH::BodyInterface& interface = physics.GetBodyInterface();

        world.Query<Collider, Transform>([&](Entity entity, const Collider& collider,
                                             const Transform& transform) {
            const RigidBody* body = world.Get<RigidBody>(entity);
            const u64 key = PackEntity(entity);
            live.insert(key);
            const u64 stamp = Fingerprint(collider, body, transform);
            const auto existing = bodies.find(key);
            if (existing != bodies.end() && fingerprints[key] != stamp) {
                DestroyBody(key);
            }
            if (bodies.find(key) == bodies.end()) {
                JPH::RefConst<JPH::Shape> shape = MakeShape(collider, transform);
                const JPH::EMotionType motion = MotionTypeOf(body);
                const ObjectLayer layer =
                    motion == JPH::EMotionType::Static ? kLayerNonMoving : kLayerMoving;
                JPH::BodyCreationSettings created(shape, ToJolt(transform.position),
                                                  QuatFromTransform(transform), motion, layer);
                created.mUserData = key;
                created.mIsSensor = collider.isTrigger;
                if (body != nullptr) {
                    created.mFriction = std::max(body->friction, 0.0f);
                    created.mRestitution = std::max(body->restitution, 0.0f);
                    created.mLinearDamping = std::max(body->linearDamping, 0.0f);
                    created.mAngularDamping = std::max(body->angularDamping, 0.0f);
                    created.mGravityFactor = body->gravityScale;
                    if (body->mass > 0.0f && motion == JPH::EMotionType::Dynamic) {
                        created.mOverrideMassProperties =
                            JPH::EOverrideMassProperties::CalculateInertia;
                        created.mMassPropertiesOverride.mMass = body->mass;
                    }
                    if (body->lockRotation) {
                        created.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
                                               JPH::EAllowedDOFs::TranslationY |
                                               JPH::EAllowedDOFs::TranslationZ;
                    }
                }
                const JPH::BodyID id = interface.CreateAndAddBody(
                    created, motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate
                                                               : JPH::EActivation::Activate);
                bodies[key] = id;
                fingerprints[key] = stamp;
            } else if (MotionTypeOf(body) != JPH::EMotionType::Dynamic) {
                interface.SetPositionAndRotation(bodies[key], ToJolt(transform.position),
                                                 QuatFromTransform(transform),
                                                 JPH::EActivation::DontActivate);
            }
            if (body != nullptr && body->applyVelocity && bodies.contains(key)) {
                interface.SetLinearVelocity(bodies[key], ToJolt(body->linearVelocity));
                interface.SetAngularVelocity(bodies[key], ToJolt(body->angularVelocity));
            }
        });

        std::vector<u64> stale;
        for (const auto& [key, id] : bodies) {
            if (!live.contains(key)) {
                stale.push_back(key);
            }
        }
        for (const u64 key : stale) {
            DestroyBody(key);
        }
    }

    void WriteBodies(Scene& scene)
    {
        World& world = scene.GetWorld();
        JPH::BodyInterface& interface = physics.GetBodyInterface();
        world.Query<Collider, Transform>([&](Entity entity, const Collider&, Transform& transform) {
            RigidBody* body = world.Get<RigidBody>(entity);
            if (body == nullptr || body->motion != BodyMotion::Dynamic) {
                return;
            }
            const u64 key = PackEntity(entity);
            const auto found = bodies.find(key);
            if (found == bodies.end()) {
                return;
            }
            JPH::RVec3 position{};
            JPH::Quat rotation{};
            interface.GetPositionAndRotation(found->second, position, rotation);
            transform.position = FromJolt(position);
            transform.rotation = EulerFromQuat(rotation);
            body->linearVelocity = FromJolt(interface.GetLinearVelocity(found->second));
            body->angularVelocity = FromJolt(interface.GetAngularVelocity(found->second));
            body->applyVelocity = false;
        });
    }

    void SyncCharacters(Scene& scene)
    {
        World& world = scene.GetWorld();
        std::unordered_set<u64> live;
        world.Query<CharacterMotor, Transform>(
            [&](Entity entity, const CharacterMotor& motor, const Transform& transform) {
                if (!motor.enabled) {
                    return;
                }
                const u64 key = PackEntity(entity);
                live.insert(key);
                if (characters.contains(key)) {
                    return;
                }
                const f32 radius = std::max(motor.radius, 0.05f);
                const f32 halfCylinder =
                    std::max(motor.height * 0.5f - radius, 0.05f);
                JPH::CharacterVirtualSettings settings{};
                settings.mMaxSlopeAngle = Radians(motor.maxSlopeDegrees);
                settings.mShape = new JPH::CapsuleShape(halfCylinder, radius);
                settings.mInnerBodyShape = settings.mShape;
                settings.mInnerBodyLayer = kLayerMoving;
                JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
                    &settings, ToJolt(transform.position), QuatFromTransform(transform), 0,
                    &physics);
                character->SetUserData(key);
                characters[key] = character;
            });

        std::vector<u64> stale;
        for (const auto& [key, character] : characters) {
            if (!live.contains(key)) {
                stale.push_back(key);
            }
        }
        for (const u64 key : stale) {
            characters.erase(key);
        }
    }

    void StepCharacters(Scene& scene, f32 deltaSeconds)
    {
        World& world = scene.GetWorld();
        const JPH::Vec3 gravity = physics.GetGravity();
        world.Query<CharacterMotor, Transform>(
            [&](Entity entity, CharacterMotor& motor, Transform& transform) {
                if (!motor.enabled) {
                    return;
                }
                const auto found = characters.find(PackEntity(entity));
                if (found == characters.end()) {
                    return;
                }
                JPH::CharacterVirtual& character = *found->second;
                character.SetRotation(QuatFromTransform(transform));
                // Wish owns the ground plane; keep the solved vertical speed so
                // a jump is not cancelled on the next frame, and so gravity
                // that ExtendedUpdate writes back survives into the next step.
                JPH::Vec3 velocity = character.GetLinearVelocity();
                velocity.SetX(motor.wishVelocity.x);
                velocity.SetZ(motor.wishVelocity.z);
                if (motor.wishVelocity.y != 0.0f) {
                    velocity.SetY(motor.wishVelocity.y);
                }
                character.SetLinearVelocity(velocity);
                JPH::CharacterVirtual::ExtendedUpdateSettings update{};
                character.ExtendedUpdate(
                    deltaSeconds, gravity, update,
                    physics.GetDefaultBroadPhaseLayerFilter(kLayerMoving),
                    physics.GetDefaultLayerFilter(kLayerMoving), {}, {}, allocator);
                transform.position = FromJolt(character.GetPosition());
                motor.grounded = character.IsSupported();
            });
    }
};

PhysicsWorld::PhysicsWorld(const PhysicsSettings& settings)
{
    AcquireJolt();
    m_impl = std::make_unique<Impl>(settings);
}

PhysicsWorld::~PhysicsWorld()
{
    UnbindScene();
    if (m_impl) {
        std::vector<u64> keys;
        keys.reserve(m_impl->bodies.size());
        for (const auto& [key, id] : m_impl->bodies) {
            keys.push_back(key);
        }
        for (const u64 key : keys) {
            m_impl->DestroyBody(key);
        }
        m_impl->characters.clear();
    }
    m_impl.reset();
    ReleaseJolt();
}

void PhysicsWorld::SetGravity(Vec3 gravity)
{
    m_impl->settings.gravity = gravity;
    m_impl->physics.SetGravity(ToJolt(gravity));
}

Vec3 PhysicsWorld::Gravity() const { return m_impl->settings.gravity; }

bool PhysicsWorld::SetLinearVelocity(Entity entity, Vec3 velocity)
{
    if (!entity.IsValid() || m_impl->scene == nullptr) {
        return false;
    }
    World& world = m_impl->scene->GetWorld();
    if (RigidBody* body = world.Get<RigidBody>(entity)) {
        body->linearVelocity = velocity;
        body->applyVelocity = true;
    }
    const auto found = m_impl->bodies.find(PackEntity(entity));
    if (found == m_impl->bodies.end()) {
        return world.Get<RigidBody>(entity) != nullptr;
    }
    JPH::BodyInterface& interface = m_impl->physics.GetBodyInterface();
    interface.SetLinearVelocity(found->second, ToJolt(velocity));
    interface.ActivateBody(found->second);
    return true;
}

Vec3 PhysicsWorld::LinearVelocity(Entity entity) const
{
    const auto found = m_impl->bodies.find(PackEntity(entity));
    if (found != m_impl->bodies.end()) {
        return FromJolt(m_impl->physics.GetBodyInterface().GetLinearVelocity(found->second));
    }
    if (m_impl->scene != nullptr) {
        if (const RigidBody* body = m_impl->scene->GetWorld().Get<RigidBody>(entity)) {
            return body->linearVelocity;
        }
    }
    return {};
}

bool PhysicsWorld::SetAngularVelocity(Entity entity, Vec3 velocity)
{
    if (!entity.IsValid() || m_impl->scene == nullptr) {
        return false;
    }
    World& world = m_impl->scene->GetWorld();
    if (RigidBody* body = world.Get<RigidBody>(entity)) {
        body->angularVelocity = velocity;
        body->applyVelocity = true;
    }
    const auto found = m_impl->bodies.find(PackEntity(entity));
    if (found == m_impl->bodies.end()) {
        return world.Get<RigidBody>(entity) != nullptr;
    }
    JPH::BodyInterface& interface = m_impl->physics.GetBodyInterface();
    interface.SetAngularVelocity(found->second, ToJolt(velocity));
    interface.ActivateBody(found->second);
    return true;
}

Vec3 PhysicsWorld::AngularVelocity(Entity entity) const
{
    const auto found = m_impl->bodies.find(PackEntity(entity));
    if (found != m_impl->bodies.end()) {
        return FromJolt(m_impl->physics.GetBodyInterface().GetAngularVelocity(found->second));
    }
    if (m_impl->scene != nullptr) {
        if (const RigidBody* body = m_impl->scene->GetWorld().Get<RigidBody>(entity)) {
            return body->angularVelocity;
        }
    }
    return {};
}

bool PhysicsWorld::ApplyImpulse(Entity entity, Vec3 impulse)
{
    const auto found = m_impl->bodies.find(PackEntity(entity));
    if (found == m_impl->bodies.end()) {
        return false;
    }
    JPH::BodyInterface& interface = m_impl->physics.GetBodyInterface();
    interface.AddImpulse(found->second, ToJolt(impulse));
    interface.ActivateBody(found->second);
    return true;
}

std::vector<Entity> PhysicsWorld::OverlapSphere(Vec3 centre, f32 radius) const
{
    std::vector<Entity> hits;
    if (!std::isfinite(radius) || radius <= 0.0f || m_impl->scene == nullptr) {
        return hits;
    }
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    m_impl->physics.GetBroadPhaseQuery().CollideSphere(ToJolt(centre), radius, collector);
    const u64 worldId = m_impl->scene->GetWorld().Id();
    const JPH::BodyInterface& interface = m_impl->physics.GetBodyInterface();
    hits.reserve(collector.mHits.size());
    for (const JPH::BodyID& id : collector.mHits) {
        if (id.IsInvalid()) {
            continue;
        }
        const Entity entity = UnpackEntity(interface.GetUserData(id), worldId);
        if (entity.IsValid()) {
            hits.push_back(entity);
        }
    }
    return hits;
}

void PhysicsWorld::Step(Scene& scene, f32 deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }
    m_impl->SyncBodies(scene);
    m_impl->SyncCharacters(scene);

    const f32 fixed = m_impl->settings.fixedDeltaSeconds;
    const int steps = static_cast<int>(m_impl->settings.collisionSteps);
    if (fixed <= 0.0f) {
        m_impl->physics.Update(deltaSeconds, steps, &m_impl->allocator, &m_impl->jobs);
        m_impl->StepCharacters(scene, deltaSeconds);
    } else {
        m_impl->accumulator += deltaSeconds;
        const f32 catchup = fixed * 8.0f;
        if (m_impl->accumulator > catchup) {
            m_impl->accumulator = catchup;
        }
        while (m_impl->accumulator >= fixed) {
            m_impl->physics.Update(fixed, steps, &m_impl->allocator, &m_impl->jobs);
            m_impl->StepCharacters(scene, fixed);
            m_impl->accumulator -= fixed;
        }
    }
    m_impl->WriteBodies(scene);
}

bool PhysicsWorld::Raycast(Vec3 origin, Vec3 direction, f32 maxDistance, PhysicsRayHit& hit) const
{
    if (!std::isfinite(maxDistance) || maxDistance <= 0.0f) {
        return false;
    }
    Vec3 unit = Normalize(direction);
    if (Length(unit) <= 0.0f) {
        return false;
    }
    const JPH::RRayCast ray{ToJolt(origin), ToJolt(unit * maxDistance)};
    JPH::RayCastResult result;
    if (!m_impl->physics.GetNarrowPhaseQuery().CastRay(ray, result)) {
        return false;
    }
    const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    hit.distance = result.mFraction * maxDistance;
    hit.point = FromJolt(point);
    hit.normal = {0.0f, 1.0f, 0.0f};
    hit.entity = kInvalidEntity;
    const JPH::BodyLockRead lock(m_impl->physics.GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        hit.normal = FromJolt(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
        if (m_impl->scene != nullptr) {
            hit.entity = UnpackEntity(body.GetUserData(), m_impl->scene->GetWorld().Id());
        }
    }
    return true;
}

void PhysicsWorld::BindScene(Scene& scene)
{
    std::lock_guard<std::mutex> guard(g_bindMutex);
    if (m_impl->scene != nullptr) {
        g_bound.erase(m_impl->scene);
    }
    m_impl->scene = &scene;
    g_bound[&scene] = this;
}

void PhysicsWorld::UnbindScene()
{
    std::lock_guard<std::mutex> guard(g_bindMutex);
    if (m_impl && m_impl->scene != nullptr) {
        g_bound.erase(m_impl->scene);
        m_impl->scene = nullptr;
    }
}

PhysicsWorld* PhysicsWorld::Find(const Scene& scene)
{
    std::lock_guard<std::mutex> guard(g_bindMutex);
    const auto found = g_bound.find(&scene);
    return found == g_bound.end() ? nullptr : found->second;
}

} // namespace Concord
