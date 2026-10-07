#include "World/Behaviour/Behaviour.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Audio/AudioSubsystem.h"
#include "Engine/EngineEvents.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "Physics/PhysicsSubsystem.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/Systems/WorldContext.h"
#include "World/WorldSpec.h"

namespace Opaax
{
    // =========================================================================
    // Runtime binding
    // =========================================================================
    BehaviourSubsystem* Behaviour::RequireRuntime(const char* InWhat) const
    {
        if (m_Runtime == nullptr)
        {
            OPAAX_LOG(LogBehaviour, Warn, "{} called on a behaviour that is not running (before OnStart or after OnDestroy) — ignored",
                      InWhat);
            return nullptr;
        }
        return m_Runtime;
    }

    World& Behaviour::GetWorld() const
    {
        OPAAX_ASSERT(m_World != nullptr);
        return *m_World;
    }

    const OpaaxString& Behaviour::GetEntityName() const
    {
        static const OpaaxString EMPTY{};

        const EntityMeta* lMeta = TryGet<EntityMeta>();
        return (lMeta != nullptr) ? lMeta->Name : EMPTY;
    }

    WorldContext& Behaviour::GetContext() const
    {
        OPAAX_ASSERT(m_Runtime != nullptr);
        return m_Runtime->GetContext();
    }

    // =========================================================================
    // Transform
    // =========================================================================
    TransformComponent& Behaviour::GetTransform() const
    {
        return Get<TransformComponent>();
    }

    Vector2F Behaviour::GetPosition() const
    {
        return GetTransform().Position;
    }

    void Behaviour::SetPosition(const Vector2F& InPosition)
    {
        GetTransform().Position = InPosition;
        m_World->MarkChanged();
    }

    float Behaviour::GetRotation() const
    {
        return GetTransform().Rotation;
    }

    void Behaviour::SetRotation(const float InDegrees)
    {
        GetTransform().Rotation = InDegrees;
        m_World->MarkChanged();
    }

    Vector2F Behaviour::GetWorldPosition() const
    {
        return EntityHierarchy::WorldTransform(GetEntity()).Position;
    }

    void Behaviour::SetWorldPosition(const Vector2F& InPosition)
    {
        TransformComponent lWorldPose = EntityHierarchy::WorldTransform(GetEntity());
        lWorldPose.Position = InPosition;
        EntityHierarchy::SetWorldTransform(GetEntity(), lWorldPose);
        m_World->MarkChanged();
    }

    // =========================================================================
    // Creating and destroying
    // =========================================================================
    void Behaviour::Destroy()
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("Destroy"))
        {
            lRuntime->DestroyLater(m_Entity);
        }
    }

    void Behaviour::Destroy(const Entity InEntity) const
    {
        BehaviourSubsystem* lRuntime = RequireRuntime("Destroy");
        if (lRuntime == nullptr)
        {
            return;
        }

        if (InEntity.GetWorld() != m_World)
        {
            OPAAX_LOG(LogBehaviour, Warn, "Destroy: the entity is not in this behaviour's world — ignored");
            return;
        }

        lRuntime->DestroyLater(InEntity.GetHandle());
    }

    void Behaviour::DestroyAfter(const float InSeconds)
    {
        BehaviourSubsystem* lRuntime = RequireRuntime("DestroyAfter");
        if (lRuntime == nullptr)
        {
            return;
        }

        const EntityID lEntity = m_Entity;
        lRuntime->AddTimer(m_Entity, m_Type, InSeconds, /*bInRepeat*/false, nullptr,
                           [lRuntime, lEntity]() { lRuntime->DestroyLater(lEntity); });
    }

    Entity Behaviour::CreateEntity(const OpaaxString& InName) const
    {
        if (RequireRuntime("CreateEntity") == nullptr)
        {
            return Entity{};
        }

        // No owning map: runtime entities are never saved.
        return m_World->CreateEntity(InName);
    }

    Entity Behaviour::Spawn(const OpaaxString& InPrefabPath, const Vector2F& InPosition, const float InRotationDegrees) const
    {
        BehaviourSubsystem* lRuntime = RequireRuntime("Spawn");
        return (lRuntime != nullptr) ? lRuntime->SpawnPrefab(InPrefabPath, InPosition, InRotationDegrees) : Entity{};
    }

    // =========================================================================
    // Finding
    // =========================================================================
    Entity Behaviour::FindEntity(const OpaaxString& InName) const
    {
        if (m_World == nullptr)
        {
            return Entity{};
        }

        EntityID lFound = ENTITY_NONE;
        for (const auto [lEntity, lMeta] : m_World->GetRegistry().view<EntityMeta>().each())
        {
            if (lMeta.Name == InName)
            {
                lFound = lEntity;
                break;
            }
        }

        return (lFound != ENTITY_NONE) ? Entity(lFound, m_World) : Entity{};
    }

    Entity Behaviour::GetParent() const
    {
        return EntityHierarchy::GetParent(GetEntity());
    }

    void Behaviour::SetParent(const Entity InParent, const bool bInKeepWorldPose)
    {
        EntityHierarchy::SetParent(GetEntity(), InParent, bInKeepWorldPose);
    }

    // =========================================================================
    // Events
    // =========================================================================
    void Behaviour::SendErased(const EntityID InTarget, const TypeId InEventType, const void* InEvent,
                               const bool bInBubbles) const
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("Send"))
        {
            lRuntime->SendErased(InTarget, InEventType, InEvent, bInBubbles);
        }
    }

    void Behaviour::ListenErased(const TypeId InEventType, const TypeId InBehaviourType, const FBehaviourEventThunk InThunk)
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("Listen"))
        {
            lRuntime->AddListener(m_Entity, InEventType, InBehaviourType, InThunk);
        }
    }

    void Behaviour::StopPropagation() const
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("StopPropagation"))
        {
            lRuntime->StopPropagation();
        }
    }

    void Behaviour::BroadcastErased(const Uint64 InEventKey, const void* InEvent) const
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("Broadcast"))
        {
            lRuntime->GetContext().Events.GetEventBus().PublishErased(InEventKey, InEvent);
        }
    }

    void Behaviour::SubscribeErased(const Uint64 InEventKey, const TypeId InBehaviourType, const FBehaviourEventThunk InThunk)
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("Subscribe"))
        {
            lRuntime->AddBusSubscription(m_Entity, InBehaviourType, InEventKey, InThunk);
        }
    }

    // =========================================================================
    // Timers
    // =========================================================================
    TimerHandle Behaviour::SetTimer(const float InSeconds, TFunction<void()> InCallback, const bool bInRepeat)
    {
        BehaviourSubsystem* lRuntime = RequireRuntime("SetTimer");
        return (lRuntime != nullptr)
            ? lRuntime->AddTimer(m_Entity, m_Type, InSeconds, bInRepeat, nullptr, Move(InCallback))
            : TimerHandle{};
    }

    TimerHandle Behaviour::SetTimerErased(const float InSeconds, const bool bInRepeat, const TypeId InBehaviourType,
                                          const FBehaviourTimerThunk InThunk, TFunction<void()> InCallback)
    {
        BehaviourSubsystem* lRuntime = RequireRuntime("SetTimer");
        return (lRuntime != nullptr)
            ? lRuntime->AddTimer(m_Entity, InBehaviourType, InSeconds, bInRepeat, InThunk, Move(InCallback))
            : TimerHandle{};
    }

    void Behaviour::ClearTimer(const TimerHandle InHandle)
    {
        if (m_Runtime != nullptr)
        {
            m_Runtime->ClearTimer(InHandle);
        }
    }

    void Behaviour::RemoveBehaviourLater(const TypeId InBehaviourType)
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("Remove"))
        {
            lRuntime->RemoveBehaviourLater(m_Entity, InBehaviourType);
        }
    }

    // =========================================================================
    // Physics
    // =========================================================================
    void Behaviour::WarnNoBody(const char* InWhat)
    {
        if (!m_bWarnedNoBody)
        {
            m_bWarnedNoBody = true;
            OPAAX_LOG(LogBehaviour, Warn, "{} on '{}': no dynamic body (a Collider and a Dynamic Rigidbody) — ignored",
                      InWhat, GetEntityName().CStr());
        }
    }

    Vector2F Behaviour::GetVelocity() const
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        return (lPhysics != nullptr) ? lPhysics->GetLinearVelocity(m_Entity) : Vector2F{ 0.f, 0.f };
    }

    void Behaviour::SetVelocity(const Vector2F& InVelocity)
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        if (lPhysics == nullptr || !lPhysics->SetLinearVelocity(m_Entity, InVelocity))
        {
            WarnNoBody("SetVelocity");
        }
    }

    float Behaviour::GetAngularVelocity() const
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        return (lPhysics != nullptr) ? lPhysics->GetAngularVelocity(m_Entity) : 0.f;
    }

    void Behaviour::SetAngularVelocity(const float InDegreesPerSecond)
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        if (lPhysics == nullptr || !lPhysics->SetAngularVelocity(m_Entity, InDegreesPerSecond))
        {
            WarnNoBody("SetAngularVelocity");
        }
    }

    void Behaviour::AddForce(const Vector2F& InForce)
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        if (lPhysics == nullptr || !lPhysics->ApplyForce(m_Entity, InForce))
        {
            WarnNoBody("AddForce");
        }
    }

    void Behaviour::AddImpulse(const Vector2F& InImpulse)
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        if (lPhysics == nullptr || !lPhysics->ApplyImpulse(m_Entity, InImpulse))
        {
            WarnNoBody("AddImpulse");
        }
    }

    void Behaviour::AddTorque(const float InTorque)
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        if (lPhysics == nullptr || !lPhysics->ApplyTorque(m_Entity, InTorque))
        {
            WarnNoBody("AddTorque");
        }
    }

    void Behaviour::AddAngularImpulse(const float InImpulse)
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        if (lPhysics == nullptr || !lPhysics->ApplyAngularImpulse(m_Entity, InImpulse))
        {
            WarnNoBody("AddAngularImpulse");
        }
    }

    float Behaviour::GetMass() const
    {
        PhysicsSubsystem* lPhysics = GetSubsystem<PhysicsSubsystem>();
        return (lPhysics != nullptr) ? lPhysics->GetMass(m_Entity) : 0.f;
    }

    // =========================================================================
    // Audio
    // =========================================================================
    SoundHandle Behaviour::PlaySound(const OpaaxString& InClipPath, const float InVolume, const float InPitch) const
    {
        AudioSubsystem* lAudio = GetSubsystem<AudioSubsystem>();
        if (lAudio == nullptr)
        {
            return SoundHandle{};
        }

        PlaySoundParams lParams;
        lParams.Volume = InVolume;
        lParams.Pitch  = InPitch;
        return lAudio->PlaySound(InClipPath, lParams);
    }

    SoundHandle Behaviour::PlaySoundAt(const OpaaxString& InClipPath, const Vector2F& InPosition, const float InVolume) const
    {
        AudioSubsystem* lAudio = GetSubsystem<AudioSubsystem>();
        if (lAudio == nullptr)
        {
            return SoundHandle{};
        }

        PlaySoundParams lParams;
        lParams.Volume   = InVolume;
        lParams.bSpatial = true;
        lParams.Position = InPosition;
        return lAudio->PlaySound(InClipPath, lParams);
    }

    void Behaviour::StopSound(const SoundHandle InSound) const
    {
        if (AudioSubsystem* lAudio = GetSubsystem<AudioSubsystem>())
        {
            lAudio->Stop(InSound);
        }
    }

    void Behaviour::PlayAudioSource() const
    {
        if (AudioSubsystem* lAudio = GetSubsystem<AudioSubsystem>())
        {
            lAudio->PlaySource(m_Entity);
        }
    }

    void Behaviour::StopAudioSource() const
    {
        if (AudioSubsystem* lAudio = GetSubsystem<AudioSubsystem>())
        {
            lAudio->StopSource(m_Entity);
        }
    }

    // =========================================================================
    // Input
    // =========================================================================
    const InputManager& Behaviour::GetInput() const
    {
        return GetContext().Input;
    }

    bool Behaviour::IsKeyDown(const EKeyCode InKey) const
    {
        return m_Runtime != nullptr && GetInput().IsKeyDown(InKey);
    }

    bool Behaviour::WasKeyPressed(const EKeyCode InKey) const
    {
        return m_Runtime != nullptr && GetInput().WasPressedThisFrame(InKey);
    }

    bool Behaviour::WasKeyReleased(const EKeyCode InKey) const
    {
        return m_Runtime != nullptr && GetInput().WasReleasedThisFrame(InKey);
    }

    // =========================================================================
    // Input actions
    // =========================================================================
    const InputActionState* Behaviour::FindActionState(const OpaaxStringID InAction) const
    {
        const InputMappingSubsystem* lActions = (m_Runtime != nullptr) ? m_Runtime->GetContext().Actions : nullptr;
        return (lActions != nullptr) ? lActions->FindState(InAction) : nullptr;
    }

    InputActionValue Behaviour::GetAction(const OpaaxStringID InAction) const
    {
        const InputActionState* lState = FindActionState(InAction);
        return (lState != nullptr) ? lState->Value : InputActionValue{};
    }

    bool Behaviour::IsActionActive(const OpaaxStringID InAction) const
    {
        const InputActionState* lState = FindActionState(InAction);
        return lState != nullptr && lState->bTriggered;
    }

    bool Behaviour::WasActionStarted(const OpaaxStringID InAction) const
    {
        const InputActionState* lState = FindActionState(InAction);
        return lState != nullptr && lState->bStarted;
    }

    bool Behaviour::WasActionCompleted(const OpaaxStringID InAction) const
    {
        const InputActionState* lState = FindActionState(InAction);
        return lState != nullptr && lState->bCompleted;
    }

    void Behaviour::BindActionErased(const OpaaxStringID InAction, const EInputTrigger InTrigger,
                                     const TypeId InBehaviourType, const FBehaviourEventThunk InThunk)
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("BindAction"))
        {
            lRuntime->AddActionBinding(m_Entity, InBehaviourType, InAction, InTrigger, InThunk);
        }
    }

    // =========================================================================
    // Time and game
    // =========================================================================
    bool Behaviour::IsWorldEnding() const
    {
        return m_Runtime != nullptr && m_Runtime->IsEnding();
    }

    double Behaviour::GetTime() const
    {
        return (m_Runtime != nullptr) ? m_Runtime->GetTime() : 0.0;
    }

    float Behaviour::GetDeltaTime() const
    {
        return (m_Runtime != nullptr) ? m_Runtime->GetDeltaTime() : 0.f;
    }

    void Behaviour::OpenLevel(const OpaaxString& InLevelPath) const
    {
        if (RequireRuntime("OpenLevel") == nullptr)
        {
            return;
        }

        WorldSpec lSpec;
        lSpec.LevelPath = InLevelPath;
        lSpec.Mode      = EWorldMode::Play;
        OpaaxApplication::GetAppService<IEngine>().RequestOpenLevel(lSpec);
    }

    void Behaviour::QuitGame() const
    {
        if (BehaviourSubsystem* lRuntime = RequireRuntime("QuitGame"))
        {
            // Queued: the host may tear this world down, which cannot happen inside its update.
            lRuntime->GetContext().Events.GetEventBus().Enqueue(QuitGameRequested{});
        }
    }

    DebugDraw& Behaviour::GetDebugDraw() const
    {
        return GetContext().Debug;
    }
}
