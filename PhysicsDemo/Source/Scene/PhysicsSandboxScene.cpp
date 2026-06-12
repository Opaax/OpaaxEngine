#include "PhysicsSandboxScene.h"

#include <filesystem>

#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Scene/SceneSerializer.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/MoverComponent.h"

#include "Physics/Collision/CollisionProfile.h"

using namespace Opaax;
using namespace Opaax::ECS;

namespace
{
    constexpr const char* kSavePath = "PhysicsDemo/Assets/Scenes/PhysicsSandbox.scene.json";

    // Visual size for a box/circle == its collider extents, so the sprite reads as the body.
    // Transform.Scale stays {1,1}; SpriteComponent.Size carries the world-space size.
    void AddSprite(World& InWorld, EntityID InEntity, const TextureHandle& InTex, Vector2F InSize, Vector4F InColor)
    {
        SpriteComponent lSprite(InTex);
        lSprite.Size  = InSize;
        lSprite.Color = InColor;
        InWorld.AddComponent<SpriteComponent>(InEntity, lSprite);
    }

    TAssetHandle<CollisionProfile> LoadProfile(const char* InLogicalId)
    {
        return AssetRegistry::Load<CollisionProfile>(OpaaxStringID(InLogicalId));
    }

    // Static box: a lone collider builds as an implicit static body (no RigidbodyComponent).
    EntityID MakeStaticBox(World& InWorld, const char* InTag, const TextureHandle& InTex,
                           Vector2F InPos, Vector2F InSize, float InRotation, Vector4F InColor,
                           const char* InProfile)
    {
        const EntityID lE = InWorld.CreateEntity(InTag);

        TransformComponent lTr({ InPos.x, InPos.y }, { 1.f, 1.f });
        lTr.Rotation = InRotation;
        InWorld.AddComponent<TransformComponent>(lE, lTr);

        AddSprite(InWorld, lE, InTex, InSize, InColor);

        ColliderComponent lCol;
        lCol.Shape   = EColliderShape::Box;
        lCol.Mode    = EColliderMode::Solid;
        lCol.Channel = ECollisionChannel::WorldStatic;
        lCol.Size    = InSize;
        lCol.Profile = LoadProfile(InProfile);
        InWorld.AddComponent<ColliderComponent>(lE, lCol);

        return lE;
    }

    // Dynamic box with author-tunable material (friction/restitution exercise the wired material path).
    EntityID MakeDynamicBox(World& InWorld, const char* InTag, const TextureHandle& InTex,
                            Vector2F InPos, Vector2F InSize, Vector4F InColor,
                            float InFriction, float InRestitution)
    {
        const EntityID lE = InWorld.CreateEntity(InTag);

        InWorld.AddComponent<TransformComponent>(lE, TransformComponent({ InPos.x, InPos.y }, { 1.f, 1.f }));
        AddSprite(InWorld, lE, InTex, InSize, InColor);

        InWorld.AddComponent<RigidbodyComponent>(lE, RigidbodyComponent(EBodyType::Dynamic));

        ColliderComponent lCol;
        lCol.Shape       = EColliderShape::Box;
        lCol.Mode        = EColliderMode::Solid;
        lCol.Channel     = ECollisionChannel::WorldDynamic;
        lCol.Size        = InSize;
        lCol.Friction    = InFriction;
        lCol.Restitution = InRestitution;
        lCol.Profile     = LoadProfile("Physics/DynamicBox");
        InWorld.AddComponent<ColliderComponent>(lE, lCol);

        return lE;
    }

    // Static circular sensor (Mode=Overlap). Tagged "Pickup" so PhysicsEventSystem consumes it on
    // first overlap — the destroy-in-handler that exercises the engine's live body-removal reconcile.
    EntityID MakePickupSensor(World& InWorld, const TextureHandle& InTex, Vector2F InPos, float InRadius)
    {
        const EntityID lE = InWorld.CreateEntity("Pickup");
        InWorld.AddComponent<TransformComponent>(lE, TransformComponent({ InPos.x, InPos.y }, { 1.f, 1.f }));
        AddSprite(InWorld, lE, InTex, { InRadius * 2.f, InRadius * 2.f }, { 1.0f, 0.90f, 0.20f, 1.f });

        ColliderComponent lCol;
        lCol.Shape   = EColliderShape::Circle;
        lCol.Mode    = EColliderMode::Overlap;
        lCol.Channel = ECollisionChannel::Trigger;
        lCol.Radius  = InRadius;
        lCol.Profile = LoadProfile("Physics/Pickup");
        InWorld.AddComponent<ColliderComponent>(lE, lCol);
        return lE;
    }
}

void PhysicsSandboxScene::OnLoad(World& InWorld)
{
    OPAAX_TRACE("[PhysicsSandboxScene] OnLoad");

    const OpaaxString lSavePath = OpaaxPath::ToAbsolute(kSavePath);
    if (std::filesystem::exists(lSavePath.CStr()))
    {
        OPAAX_TRACE("[PhysicsSandboxScene] Loading from file: {}", lSavePath);
        SceneSerializer::Deserialize(*this, lSavePath.CStr(), InWorld);
        SetSourcePath(lSavePath);
        return;
    }

    OPAAX_TRACE("[PhysicsSandboxScene] No save file — building from code.");
    BuildDefaultScene(InWorld);
}

void PhysicsSandboxScene::OnUnload(World& /*InWorld*/)
{
    OPAAX_TRACE("[PhysicsSandboxScene] OnUnload");
    m_SpriteTexture.Reset();
}

void PhysicsSandboxScene::SaveScene(World& InWorld)
{
    const OpaaxString lSavePath = OpaaxPath::ToAbsolute(kSavePath);
    std::filesystem::create_directories(std::filesystem::path(lSavePath.CStr()).parent_path());
    SceneSerializer::Serialize(*this, lSavePath.CStr(), InWorld);
}

void PhysicsSandboxScene::BuildDefaultScene(World& InWorld)
{
    m_SpriteTexture = AssetRegistry::Load<Texture2D>(OPAAX_ID("Textures/Player"));

    // Layout (world ≈ ±640 × ±360, Y-up): LEFT zone = player + a pickup to walk into;
    // CENTER = a box dropped over the gap (falls through an air-sensor, then past the kill-Z);
    // RIGHT zone = a plain box (rests), a bouncy ball (restitution), a ramp + low-friction box (slides).

    // --- Static geometry: two ground slabs with a central gap (the kill pit), walls, a ramp ---
    MakeStaticBox(InWorld, "GroundLeft",  m_SpriteTexture, { -330.f, -300.f }, { 440.f, 60.f }, 0.f,
                  { 0.28f, 0.30f, 0.36f, 1.f }, "Physics/Ground");
    MakeStaticBox(InWorld, "GroundRight", m_SpriteTexture, {  330.f, -300.f }, { 440.f, 60.f }, 0.f,
                  { 0.28f, 0.30f, 0.36f, 1.f }, "Physics/Ground");
    MakeStaticBox(InWorld, "LeftWall",    m_SpriteTexture, { -565.f,  -60.f }, {  40.f, 480.f }, 0.f,
                  { 0.40f, 0.42f, 0.48f, 1.f }, "Physics/Ground");
    MakeStaticBox(InWorld, "RightWall",   m_SpriteTexture, {  565.f,  -60.f }, {  40.f, 480.f }, 0.f,
                  { 0.40f, 0.42f, 0.48f, 1.f }, "Physics/Ground");
    // Negative rotation tilts the +x end DOWN, so the ice box slides RIGHT into the wall corner
    // (never toward the central gap). Sits within GroundRight's span.
    MakeStaticBox(InWorld, "Ramp",        m_SpriteTexture, {  410.f, -228.f }, { 220.f, 22.f }, -0.30f,
                  { 0.52f, 0.42f, 0.30f, 1.f }, "Physics/Ground");

    // --- LEFT zone: player walks right into the pickup ---
    {
        const EntityID lPlayer = InWorld.CreateEntity("Player");
        InWorld.AddComponent<TransformComponent>(lPlayer, TransformComponent({ -460.f, 60.f }, { 1.f, 1.f }));
        AddSprite(InWorld, lPlayer, m_SpriteTexture, { 50.f, 100.f }, { 0.30f, 0.90f, 0.42f, 1.f });

        MoverComponent& lMover = InWorld.AddComponent<MoverComponent>(lPlayer);
        lMover.Shape   = EMoverShape::Capsule;
        lMover.Height  = 100.f;
        lMover.Radius  = 25.f;
        lMover.Modes   = { OPAAX_ID("GroundMove"), OPAAX_ID("Fly") };
        lMover.ModeId  = OPAAX_ID("GroundMove");
        lMover.Profile = LoadProfile("Physics/Pawn");
    }
    MakePickupSensor(InWorld, m_SpriteTexture, { -210.f, -235.f }, 35.f);

    // --- CENTER: box over the gap. Falls through an air-sensor (consumed → exercises the live
    //     body-removal reconcile), then keeps falling past the kill-Z (world-bounds auto-destroy). ---
    MakePickupSensor(InWorld, m_SpriteTexture, { 0.f, 60.f }, 40.f);
    MakeDynamicBox(InWorld, "PitBox", m_SpriteTexture, { 0.f, 260.f }, { 50.f, 50.f },
                   { 0.90f, 0.32f, 0.80f, 1.f }, /*friction*/ 0.3f, /*restitution*/ 0.f);

    // --- RIGHT zone: plain rest vs bounce vs low-friction slide ---
    MakeDynamicBox(InWorld, "RestBox", m_SpriteTexture, { 170.f, 280.f }, { 60.f, 60.f },
                   { 0.30f, 0.52f, 0.95f, 1.f }, /*friction*/ 0.3f, /*restitution*/ 0.f);

    {
        const EntityID lBall = InWorld.CreateEntity("BouncyBall");
        InWorld.AddComponent<TransformComponent>(lBall, TransformComponent({ 250.f, 320.f }, { 1.f, 1.f }));
        AddSprite(InWorld, lBall, m_SpriteTexture, { 56.f, 56.f }, { 1.0f, 0.45f, 0.20f, 1.f });
        InWorld.AddComponent<RigidbodyComponent>(lBall, RigidbodyComponent(EBodyType::Dynamic));
        ColliderComponent lCol;
        lCol.Shape       = EColliderShape::Circle;
        lCol.Mode        = EColliderMode::Solid;
        lCol.Channel     = ECollisionChannel::WorldDynamic;
        lCol.Radius      = 28.f;
        lCol.Restitution = 0.7f;
        lCol.Profile     = LoadProfile("Physics/DynamicBox");
        InWorld.AddComponent<ColliderComponent>(lBall, lCol);
    }

    MakeDynamicBox(InWorld, "IceBox", m_SpriteTexture, { 360.f, 40.f }, { 40.f, 40.f },
                   { 0.40f, 0.90f, 0.95f, 1.f }, /*friction*/ 0.02f, /*restitution*/ 0.f);

    OPAAX_CORE_INFO("[PhysicsSandboxScene] Built sandbox. Controls: A/D move, Space jump, "
                    "Tab Ground/Fly, Q raycast-down, E overlap-here.");
}
