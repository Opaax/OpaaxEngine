#include "SandboxControlSystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/ApplicationEvents.hpp"
#include "Core/Event/OpaaxEventDispatcher.hpp"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Input/OpaaxInputEvents.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxStringID.hpp"

#include "Core/Systems/PhysicsSubsystem.h"
#include "World/World.h"
#include "Assets/AssetRegistry.h"
#include "ECS/Components/MoverComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/TagComponent.h"
#include "Physics/Collision/CollisionProfile.h"

#include "Renderer/Camera/CameraControllerSystem.h"
#include "Renderer/Camera/FollowCameraController.h"
#include "Renderer/Camera/FollowParams.h"
#include "Renderer/RenderSubsystem.h"
#include "Renderer/Camera/ICamera.h"
#include "Renderer/Camera/OrthographicCamera.h"

using namespace Opaax;
using namespace Opaax::ECS;

namespace
{
    OpaaxString TagName(World& InWorld, EntityID InEntity)
    {
        if (const TagComponent* lTag = InWorld.GetComponent<TagComponent>(InEntity))
        {
            return lTag->Tag.ToString();
        }
        return OpaaxString("<none>");
    }
}

Uint32 SandboxControlSystem::GetEventCategoryFilter() const noexcept
{
    return EEventCategory_Keyboard | EEventCategory_Application | EEventCategory_Mouse;
}

bool SandboxControlSystem::OnEvent(OpaaxEvent& InEvent)
{
    OpaaxEventDispatcher lDispatcher(InEvent);

    lDispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& E) -> bool
    {
        if (E.IsRepeat()) { return false; }
        switch (E.GetKeyCode())
        {
            case EOpaaxKeyCode::A:     m_Left  = true; break;
            case EOpaaxKeyCode::D:     m_Right = true; break;
            case EOpaaxKeyCode::W:     m_Up    = true; break;
            case EOpaaxKeyCode::S:     m_Down  = true; break;
            case EOpaaxKeyCode::Space: m_JumpQueued       = true; break;
            case EOpaaxKeyCode::Tab:   m_ToggleModeQueued = true; break;
            case EOpaaxKeyCode::Q:     m_RaycastQueued    = true; break;
            case EOpaaxKeyCode::E:     m_OverlapQueued    = true; break;
            case EOpaaxKeyCode::R:     m_SpawnQueued      = true; break;
            default: break;
        }
        return false;
    });

    lDispatcher.Dispatch<KeyReleasedEvent>([this](KeyReleasedEvent& E) -> bool
    {
        switch (E.GetKeyCode())
        {
            case EOpaaxKeyCode::A: m_Left  = false; break;
            case EOpaaxKeyCode::D: m_Right = false; break;
            case EOpaaxKeyCode::W: m_Up    = false; break;
            case EOpaaxKeyCode::S: m_Down  = false; break;
            default: break;
        }
        return false;
    });
    
    lDispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent ScrollEvent)-> bool
    {
        ICamera& lActive = GetEngineApp()->GetSubsystem<RenderSubsystem>()->GetActiveCamera();
        if (OrthographicCamera* lOrtho = static_cast<OrthographicCamera*>(&lActive))
        {
            constexpr float LZoomStep = 1.1f;
            constexpr float LZoomMin  = 0.1f;
            constexpr float LZoomMax  = 10.f;
            
            float lCurZoom = lOrtho->GetZoom();
            const float lNewZoom = std::clamp(lCurZoom * std::pow(LZoomStep, ScrollEvent.GetYOffset()), LZoomMin, LZoomMax);
            
            lOrtho->SetZoom(lNewZoom);
        }
        return false;
    });

    lDispatcher.Dispatch<WindowLostFocusEvent>([this](WindowLostFocusEvent&) -> bool
    {
        m_Left = m_Right = m_Up = m_Down = false;
        return false;
    });

    return false;
}

void SandboxControlSystem::Update(double /*DeltaTime*/)
{
    CoreEngineApp* lApp = GetEngineApp();
    if (lApp == nullptr) { return; }

    World& lWorld = lApp->GetWorld();

    // One MoverComponent in this scene (the player). Re-found each frame so a PIE restart that
    // rebuilds the world reattaches transparently. Mover-coupled actions run inside the view;
    // entity-creating / camera work happens AFTER (never create entities mid view-iteration).
    EntityID lPlayer    = ENTITY_NONE;
    Vector2F lPlayerPos = { 0.f, 0.f };

    lWorld.Each<MoverComponent, TransformComponent>(
        [this, &lPlayer, &lPlayerPos](EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform)
        {
            lPlayer    = InEntity;
            lPlayerPos = InTransform.Position;

            const float lX = (m_Right ? 1.f : 0.f) - (m_Left ? 1.f : 0.f);
            const float lY = (m_Up    ? 1.f : 0.f) - (m_Down ? 1.f : 0.f);   // Fly only; Ground ignores Y
            InMover.Input.MoveDir = { lX, lY };

            if (m_JumpQueued)
            {
                InMover.Input.Jump = true;   // consumed + cleared by GroundMoveMode on the next tick
                m_JumpQueued = false;
            }

            if (m_ToggleModeQueued)
            {
                m_ToggleModeQueued = false;
                const OpaaxStringID lNext = (InMover.GetCurrentMode() == OPAAX_ID("GroundMove"))
                                          ? OPAAX_ID("Fly") : OPAAX_ID("GroundMove");
                InMover.QueueNextMode(lNext);
                const OpaaxString lName = lNext.ToString();
                OPAAX_CORE_INFO("[Sandbox] Mover mode -> {}", lName);
            }

            if (m_RaycastQueued) { m_RaycastQueued = false; DoRaycastDown(InTransform.Position); }
            if (m_OverlapQueued) { m_OverlapQueued = false; DoOverlapHere(InTransform.Position); }
        });

    if (lPlayer == ENTITY_NONE) { return; }

    // (Re)point the follow camera at the player when its id changes (first frame / PIE restart).
    if (lPlayer != m_CameraTarget)
    {
        AttachFollowCamera(lPlayer);
        m_CameraTarget = lPlayer;
    }

    // Spawn outside the view iteration — creating an entity adds a TransformComponent, which
    // would mutate the storage being iterated above.
    if (m_SpawnQueued)
    {
        m_SpawnQueued = false;
        SpawnDynamicBox({ lPlayerPos.x, lPlayerPos.y + 320.f });
    }
}

void SandboxControlSystem::AttachFollowCamera(EntityID InTarget)
{
    CameraControllerSystem* lCam = GetEngineApp()->GetGameSubsystem<CameraControllerSystem>();
    if (lCam == nullptr) { return; }

    lCam->DetachAll();
    lCam->AttachController(MakeUnique<FollowCameraController>(
        FollowParams{ /*Target*/ InTarget, /*Offset*/ { 0.f, 0.f },
                      /*Smoothing*/ 0.15f, /*Deadzone*/ { 40.f, 40.f } },
        GetEngineApp()->GetWorld()));
    
    OPAAX_CORE_INFO("[Sandbox] Follow camera attached to player.");
}

void SandboxControlSystem::SpawnDynamicBox(Vector2F InAt)
{
    World& lWorld = GetEngineApp()->GetWorld();

    const EntityID lBox = lWorld.CreateEntity("SpawnBox");
    lWorld.AddComponent<TransformComponent>(lBox, TransformComponent({ InAt.x, InAt.y }, { 1.f, 1.f }));

    SpriteComponent lSprite(AssetRegistry::Load<Texture2D>(OPAAX_ID("Textures/Player")));
    lSprite.Size  = { 48.f, 48.f };
    lSprite.Color = { 0.95f, 0.85f, 0.20f, 1.f };
    lWorld.AddComponent<SpriteComponent>(lBox, lSprite);

    lWorld.AddComponent<RigidbodyComponent>(lBox, RigidbodyComponent(EBodyType::Dynamic));

    ColliderComponent lCol;
    lCol.Shape   = EColliderShape::Box;
    lCol.Mode    = EColliderMode::Solid;
    lCol.Channel = ECollisionChannel::WorldDynamic;
    lCol.Size    = { 48.f, 48.f };
    lCol.Profile = AssetRegistry::Load<CollisionProfile>(OPAAX_ID("Physics/DynamicBox"));
    lWorld.AddComponent<ColliderComponent>(lBox, lCol);

    // No body created here — PhysicsSubsystem::ReconcileNewBodies builds it next fixed step.
    OPAAX_CORE_INFO("[Sandbox] Spawned runtime dynamic box at ({}, {}).", InAt.x, InAt.y);
}

void SandboxControlSystem::DoRaycastDown(Vector2F InOrigin)
{
    PhysicsSubsystem* lPhysics = GetEngineApp()->GetSubsystem<PhysicsSubsystem>();
    if (lPhysics == nullptr) { return; }

    const PhysicsSubsystem::RaycastResult lHit = lPhysics->RayCast(InOrigin, { 0.f, -1.f }, 600.f);
    if (lHit.bHit)
    {
        const OpaaxString lName = TagName(GetEngineApp()->GetWorld(), lHit.Entity);
        OPAAX_CORE_INFO("[Sandbox] Raycast down hit '{}' at ({}, {}) frac {}",
                        lName, lHit.Point.x, lHit.Point.y, lHit.Fraction);
    }
    else
    {
        OPAAX_CORE_INFO("[Sandbox] Raycast down hit nothing.");
    }
}

void SandboxControlSystem::DoOverlapHere(Vector2F InCenter)
{
    PhysicsSubsystem* lPhysics = GetEngineApp()->GetSubsystem<PhysicsSubsystem>();
    if (lPhysics == nullptr) { return; }

    TDynArray<EntityID> lHits;
    lPhysics->OverlapAABB({ InCenter.x - 120.f, InCenter.y - 120.f },
                          { InCenter.x + 120.f, InCenter.y + 120.f }, lHits);

    World& lWorld = GetEngineApp()->GetWorld();
    OPAAX_CORE_INFO("[Sandbox] Overlap query found {} entities:", lHits.size());
    for (const EntityID lE : lHits)
    {
        const OpaaxString lName = TagName(lWorld, lE);
        OPAAX_CORE_INFO("    - {}", lName);
    }
}
