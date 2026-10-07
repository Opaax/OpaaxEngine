#include "World/Behaviour/BehaviourSubsystem.h"

#include <algorithm>

#include "Core/Events/EventBus.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Physics/PhysicsEvents.h"
#include "Resources/ResourceManager.h"
#include "World/Behaviour/EntityEvents.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/ResourcePrefabResolver.h"
#include "World/Serialization/MapFactory.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** Nested Send calls allowed before an event is dropped (a handler sending back and forth). */
        constexpr Uint32 MAX_SEND_DEPTH = 32;

        Uint32 EntityKey(const EntityID InEntity) noexcept
        {
            return static_cast<Uint32>(InEntity);
        }
    }

    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    bool BehaviourSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    BehaviourSubsystem::BehaviourSubsystem(WorldContext& InContext)
        : m_Context(&InContext)
    {
    }

    BehaviourSubsystem::~BehaviourSubsystem() = default;

    // =========================================================================
    // ISubsystem
    // =========================================================================
    bool BehaviourSubsystem::Startup()
    {
        if (m_Context->Components == nullptr)
        {
            OPAAX_LOG(LogBehaviour, Error, "World '{}' has no component registry: its behaviours will not run.",
                      m_Context->OwningWorld.GetName().CStr());
            return true;
        }

        EntityRegistry& lRegistry = m_Context->OwningWorld.GetRegistry();

        m_Context->Components->ForEach([&](const IComponentEntry& InEntry)
        {
            if (!InEntry.IsBehaviour())
            {
                return;
            }

            m_Buckets.push_back(TypeBucket{ InEntry.GetTypeId(), &InEntry, {} });
            InEntry.ConnectBehaviourSignals(lRegistry, *this);

            // Behaviours that already exist wait for their start like new ones.
            const TypeId lType = InEntry.GetTypeId();
            InEntry.ForEachBehaviour(lRegistry, [&](const EntityID InEntity, Behaviour&)
            {
                m_Pending.push_back(PendingStart{ InEntity, lType });
            });
        });

        m_bSignalsConnected = true;
        BindPhysicsEvents();

        m_EntityDestroyingHandle = m_Context->OwningWorld.OnEntityDestroying()
                                       .AddMember(this, &BehaviourSubsystem::OnEntityDestroying);

        OPAAX_LOG(LogBehaviour, Trace, "Behaviours ready in world '{}' — {} behaviour type(s)",
                  m_Context->OwningWorld.GetName().CStr(), m_Buckets.size());
        return true;
    }

    void BehaviourSubsystem::Update(const double InDeltaTime)
    {
        m_DeltaTime = static_cast<float>(InDeltaTime);
        m_Time     += InDeltaTime;

        RunPass(/*bInFixed*/false, m_DeltaTime);
        TickTimers();
        FlushDestroyed();
    }

    void BehaviourSubsystem::FixedUpdate(const double InFixedDeltaTime)
    {
        RunPass(/*bInFixed*/true, static_cast<float>(InFixedDeltaTime));
        FlushDestroyed();
    }

    void BehaviourSubsystem::Shutdown()
    {
        if (m_bShutdown)
        {
            return;
        }
        m_bShutdown = true;

        // Every started behaviour ends before the world's entities are cleared.
        Uint64 lEnded = 0;
        for (Uint64 lBucketIndex = 0; lBucketIndex < m_Buckets.size(); ++lBucketIndex)
        {
            for (Uint64 lIndex = 0; lIndex < m_Buckets[lBucketIndex].Instances.size(); ++lIndex)
            {
                const Instance lInstance = m_Buckets[lBucketIndex].Instances[lIndex];
                if (lInstance.Ptr != nullptr && !lInstance.Ptr->m_bEnded)
                {
                    EndBehaviour(lInstance.Entity, m_Buckets[lBucketIndex].Type, *lInstance.Ptr);
                    ++lEnded;
                }
            }
        }

        if (m_EntityDestroyingHandle.IsValid())
        {
            m_Context->OwningWorld.OnEntityDestroying().Remove(m_EntityDestroyingHandle);
            m_EntityDestroyingHandle = DelegateHandle();
        }

        m_Context->Events.GetEventBus().UnsubscribeAll(this);
        m_Subscriptions.clear();
        m_Listeners.clear();
        m_Timers.clear();
        m_Pending.clear();
        m_DestroyQueue.clear();
        m_DestroyPending.clear();
        m_RemoveQueue.clear();

        if (m_bSignalsConnected)
        {
            EntityRegistry& lRegistry = m_Context->OwningWorld.GetRegistry();
            for (const TypeBucket& lBucket : m_Buckets)
            {
                lBucket.Entry->DisconnectBehaviourSignals(lRegistry, *this);
            }
            m_bSignalsConnected = false;
        }

        m_Buckets.clear();
        m_PrefabResolver.reset();

        OPAAX_LOG(LogBehaviour, Trace, "Behaviours stopped in world '{}' — {} ended",
                  m_Context->OwningWorld.GetName().CStr(), lEnded);
    }

    // =========================================================================
    // Signals
    // =========================================================================
    void BehaviourSubsystem::OnBehaviourConstructed(const EntityID InEntity, const TypeId InType)
    {
        // Its fields may not be loaded yet (a map adds the component, then reads it): start later.
        if (!m_bShutdown)
        {
            m_Pending.push_back(PendingStart{ InEntity, InType });
        }
    }

    void BehaviourSubsystem::OnBehaviourDestroyed(const EntityID InEntity, const TypeId InType, Behaviour& InBehaviour)
    {
        // A behaviour removed before it started gets no OnDestroy; its pending entry finds nothing.
        if (InBehaviour.m_bStarted && !InBehaviour.m_bEnded)
        {
            EndBehaviour(InEntity, InType, InBehaviour);
        }
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    void BehaviourSubsystem::StartPending()
    {
        // A start from inside another start is picked up by the outer loop.
        if (m_bStarting || m_bShutdown)
        {
            return;
        }
        m_bStarting = true;

        // By index: an OnStart may spawn, which queues more.
        for (Uint64 lIndex = 0; lIndex < m_Pending.size(); ++lIndex)
        {
            StartOne(m_Pending[lIndex]);
        }

        m_Pending.clear();
        m_bStarting = false;
    }

    void BehaviourSubsystem::StartOne(const PendingStart InPending)
    {
        TypeBucket* lBucket = FindBucket(InPending.Type);
        if (lBucket == nullptr)
        {
            return;
        }

        Behaviour* lBehaviour = lBucket->Entry->TryGetBehaviour(m_Context->OwningWorld.GetRegistry(), InPending.Entity);
        if (lBehaviour == nullptr || lBehaviour->m_bStarted)
        {
            return;
        }

        lBehaviour->m_Entity   = InPending.Entity;
        lBehaviour->m_World    = &m_Context->OwningWorld;
        lBehaviour->m_Runtime  = this;
        lBehaviour->m_Type     = InPending.Type;
        lBehaviour->m_bStarted = true;

        lBucket->Instances.push_back(Instance{ InPending.Entity, lBehaviour, m_PassIndex });

        lBehaviour->OnStart();
    }

    void BehaviourSubsystem::StartPendingOn(const EntityID InEntity)
    {
        // By index: an OnStart may queue more. The entries stay; the outer start skips them.
        for (Uint64 lIndex = 0; lIndex < m_Pending.size(); ++lIndex)
        {
            if (m_Pending[lIndex].Entity == InEntity)
            {
                StartOne(m_Pending[lIndex]);
            }
        }
    }

    void BehaviourSubsystem::RunPass(const bool bInFixed, const float InDeltaTime)
    {
        StartPending();

        // Behaviours started from here on are first updated in the next pass.
        const Uint64 lPass = ++m_PassIndex;

        for (Uint64 lBucketIndex = 0; lBucketIndex < m_Buckets.size(); ++lBucketIndex)
        {
            // By index and by copy: a behaviour may spawn (the vector grows) or end (Ptr goes null).
            const Uint64 lCount = m_Buckets[lBucketIndex].Instances.size();
            for (Uint64 lIndex = 0; lIndex < lCount; ++lIndex)
            {
                const Instance lInstance = m_Buckets[lBucketIndex].Instances[lIndex];
                if (lInstance.Ptr == nullptr || lInstance.Ptr->m_bEnded || lInstance.StartPass >= lPass
                    || IsDestroyPending(lInstance.Entity))
                {
                    continue;
                }

                if (bInFixed) { lInstance.Ptr->OnFixedUpdate(InDeltaTime); }
                else          { lInstance.Ptr->OnUpdate(InDeltaTime); }
            }

            StartPending();
        }

        CompactBuckets();
    }

    void BehaviourSubsystem::DestroyLater(const EntityID InEntity)
    {
        if (!m_Context->OwningWorld.IsValid(InEntity))
        {
            return;
        }

        if (m_DestroyPending.insert(EntityKey(InEntity)).second)
        {
            m_DestroyQueue.push_back(InEntity);
        }
    }

    void BehaviourSubsystem::RemoveBehaviourLater(const EntityID InEntity, const TypeId InBehaviourType)
    {
        m_RemoveQueue.push_back(PendingRemoval{ InEntity, InBehaviourType });
    }

    bool BehaviourSubsystem::IsDestroyPending(const EntityID InEntity) const
    {
        return !m_DestroyPending.empty() && m_DestroyPending.contains(EntityKey(InEntity));
    }

    void BehaviourSubsystem::FlushDestroyed()
    {
        World&          lWorld    = m_Context->OwningWorld;
        EntityRegistry& lRegistry = lWorld.GetRegistry();

        // An OnDestroy may queue more: loop until nothing is left.
        while (!m_RemoveQueue.empty() || !m_DestroyQueue.empty())
        {
            const TDynArray<PendingRemoval> lRemovals = Move(m_RemoveQueue);
            m_RemoveQueue.clear();

            for (const PendingRemoval& lRemoval : lRemovals)
            {
                const TypeBucket* lBucket = FindBucket(lRemoval.Type);
                if (lBucket == nullptr || !lWorld.IsValid(lRemoval.Entity))
                {
                    continue;
                }

                // Ended before the removal, so OnDestroy never runs inside the registry's signal.
                EndIfStarted(lRemoval.Entity, *lBucket);

                // OnDestroy may have destroyed the entity outright.
                if (lWorld.IsValid(lRemoval.Entity))
                {
                    lBucket->Entry->Remove(lRegistry, lRemoval.Entity);
                }
            }

            const TDynArray<EntityID> lEntities = Move(m_DestroyQueue);
            m_DestroyQueue.clear();

            for (const EntityID lEntity : lEntities)
            {
                // A parent destroyed earlier in this loop already took its children.
                if (lWorld.IsValid(lEntity))
                {
                    lWorld.DestroyEntity(lEntity);
                }
            }
        }

        m_DestroyPending.clear();
        CompactBuckets();
    }

    void BehaviourSubsystem::OnEntityDestroying(const EntityID InEntity)
    {
        for (const TypeBucket& lBucket : m_Buckets)
        {
            EndIfStarted(InEntity, lBucket);
        }
    }

    void BehaviourSubsystem::EndIfStarted(const EntityID InEntity, const TypeBucket& InBucket)
    {
        Behaviour* lBehaviour = InBucket.Entry->TryGetBehaviour(m_Context->OwningWorld.GetRegistry(), InEntity);
        if (lBehaviour != nullptr && lBehaviour->m_bStarted && !lBehaviour->m_bEnded)
        {
            EndBehaviour(InEntity, InBucket.Type, *lBehaviour);
        }
    }

    void BehaviourSubsystem::EndBehaviour(const EntityID InEntity, const TypeId InType, Behaviour& InBehaviour)
    {
        // Marked first: it cannot end twice and receives no more events. The runtime stays bound
        // during OnDestroy (it may spawn, send, broadcast); what it registers there is forgotten next.
        InBehaviour.m_bEnded = true;
        InBehaviour.OnDestroy();

        ForgetBehaviour(InEntity, InType);
        InBehaviour.m_Runtime = nullptr;

        if (TypeBucket* lBucket = FindBucket(InType))
        {
            for (Instance& lInstance : lBucket->Instances)
            {
                if (lInstance.Ptr == &InBehaviour)
                {
                    lInstance.Ptr = nullptr;
                    break;
                }
            }
        }
    }

    void BehaviourSubsystem::ForgetBehaviour(const EntityID InEntity, const TypeId InType)
    {
        const auto lListeners = m_Listeners.find(EntityKey(InEntity));
        if (lListeners != m_Listeners.end())
        {
            TDynArray<Listener>& lList = lListeners->second;
            lList.erase(std::remove_if(lList.begin(), lList.end(),
                                       [InType](const Listener& InListener) { return InListener.BehaviourType == InType; }),
                        lList.end());
            if (lList.empty())
            {
                m_Listeners.erase(lListeners);
            }
        }

        EventBus& lBus = m_Context->Events.GetEventBus();
        for (auto lIt = m_Subscriptions.begin(); lIt != m_Subscriptions.end();)
        {
            if (lIt->Entity == InEntity && lIt->BehaviourType == InType)
            {
                lBus.Unsubscribe(lIt->Handle);
                lIt = m_Subscriptions.erase(lIt);
            }
            else
            {
                ++lIt;
            }
        }

        // Cleared, not erased: TickTimers may be iterating.
        for (Timer& lTimer : m_Timers)
        {
            if (lTimer.Entity == InEntity && lTimer.BehaviourType == InType)
            {
                lTimer.Id = 0;
            }
        }
    }

    void BehaviourSubsystem::CompactBuckets()
    {
        for (TypeBucket& lBucket : m_Buckets)
        {
            lBucket.Instances.erase(std::remove_if(lBucket.Instances.begin(), lBucket.Instances.end(),
                                                   [](const Instance& InInstance) { return InInstance.Ptr == nullptr; }),
                                    lBucket.Instances.end());
        }
    }

    // =========================================================================
    // Spawning
    // =========================================================================
    Entity BehaviourSubsystem::SpawnPrefab(const OpaaxString& InPrefabPath, const Vector2F& InPosition,
                                           const float InRotationDegrees)
    {
        if (m_bShutdown)
        {
            // An OnDestroy at the world's end: nothing is created in a world going away.
            OPAAX_LOG(LogBehaviour, Trace, "Spawn '{}' skipped: the world is ending.", InPrefabPath.CStr());
            return Entity{};
        }

        if (m_Context->Components == nullptr)
        {
            OPAAX_LOG(LogBehaviour, Error, "Spawn '{}' refused: the world has no component registry.", InPrefabPath.CStr());
            return Entity{};
        }

        if (m_PrefabResolver == nullptr)
        {
            m_PrefabResolver = MakeUnique<ResourcePrefabResolver>(m_Context->Paths, m_Context->Resources,
                                                                  *m_Context->Components);
        }

        const PrefabData* lPrefab = m_PrefabResolver->Resolve(InPrefabPath);
        if (lPrefab == nullptr)
        {
            OPAAX_LOG(LogBehaviour, Error, "Spawn: prefab '{}' could not be loaded.", InPrefabPath.CStr());
            return Entity{};
        }

        // BuildInstance stamps a map; a runtime spawn belongs to none, so it is never saved.
        MapData lInstance = PrefabFactory::BuildInstance(*lPrefab, InPrefabPath, Guid::New(), MapId("Runtime"),
                                                         *m_Context->Components);
        if (lInstance.Entities.empty())
        {
            OPAAX_LOG(LogBehaviour, Error, "Spawn: prefab '{}' has no entity.", InPrefabPath.CStr());
            return Entity{};
        }

        for (EntityData& lEntity : lInstance.Entities)
        {
            lEntity.OwnerMap = MapId();
        }

        World& lWorld = m_Context->OwningWorld;
        MapFactory::Instantiate(lInstance, lWorld, *m_Context->Components);

        // The root: the entity whose parent is not part of the instance.
        Entity lRoot;
        for (const EntityData& lEntity : lInstance.Entities)
        {
            const bool bParentInside = lEntity.Parent.IsValid()
                && std::any_of(lInstance.Entities.begin(), lInstance.Entities.end(),
                               [&](const EntityData& InOther) { return InOther.Id == lEntity.Parent; });
            if (!bParentInside)
            {
                lRoot = lWorld.FindByGuid(lEntity.Id);
                break;
            }
        }

        if (lRoot.IsValid())
        {
            TransformComponent& lTransform = lRoot.Get<TransformComponent>();
            lTransform.Position = InPosition;
            lTransform.Rotation = InRotationDegrees;
            lWorld.MarkChanged();
        }

        // The whole instance exists: its behaviours start now, before the caller's next line. Inside
        // an OnStart only the instance's start: the other pending behaviours keep their turn.
        if (m_bStarting)
        {
            for (const EntityData& lEntity : lInstance.Entities)
            {
                if (const Entity lCreated = lWorld.FindByGuid(lEntity.Id); lCreated.IsValid())
                {
                    StartPendingOn(lCreated.GetHandle());
                }
            }
        }
        else
        {
            StartPending();
        }

        return lRoot;
    }

    // =========================================================================
    // Entity events
    // =========================================================================
    void BehaviourSubsystem::SendErased(const EntityID InTarget, const TypeId InEventType, const void* InEvent,
                                        const bool bInBubbles)
    {
        if (m_bShutdown)
        {
            return;
        }

        if (m_SendDepth >= MAX_SEND_DEPTH)
        {
            if (!m_bWarnedSendDepth)
            {
                m_bWarnedSendDepth = true;
                OPAAX_LOG(LogBehaviour, Error,
                          "Event dropped: {} nested sends (handlers sending to each other in a loop?).", MAX_SEND_DEPTH);
            }
            return;
        }

        // A behaviour still waiting for its start must not miss its first event.
        StartPending();

        World& lWorld = m_Context->OwningWorld;

        ++m_SendDepth;
        const bool bOuterStop = m_bStopPropagation;
        m_bStopPropagation = false;

        EntityID lTarget = InTarget;
        for (Uint32 lDepth = 0; lDepth < EntityHierarchy::MAX_DEPTH && lWorld.IsValid(lTarget); ++lDepth)
        {
            // Sent from an OnStart: the pending behaviours are not all started yet, so the receivers start now.
            if (m_bStarting)
            {
                StartPendingOn(lTarget);
            }

            DeliverTo(lTarget, InEventType, InEvent);

            if (!bInBubbles || m_bStopPropagation)
            {
                break;
            }

            lTarget = EntityHierarchy::GetParent(Entity(lTarget, &lWorld)).GetHandle();
        }

        m_bStopPropagation = bOuterStop;
        --m_SendDepth;
    }

    void BehaviourSubsystem::DeliverTo(const EntityID InTarget, const TypeId InEventType, const void* InEvent)
    {
        const auto lFound = m_Listeners.find(EntityKey(InTarget));
        if (lFound == m_Listeners.end())
        {
            return;
        }

        // A copy: a handler may add or remove listeners.
        const TDynArray<Listener> lListeners = lFound->second;
        for (const Listener& lListener : lListeners)
        {
            if (lListener.EventType != InEventType)
            {
                continue;
            }

            // Resolved now, never stored: a behaviour that ended is simply skipped.
            if (Behaviour* lBehaviour = Resolve(InTarget, lListener.BehaviourType))
            {
                lListener.Thunk(*lBehaviour, InEvent);
            }
        }
    }

    void BehaviourSubsystem::AddListener(const EntityID InEntity, const TypeId InEventType, const TypeId InBehaviourType,
                                         const FBehaviourEventThunk InThunk)
    {
        TDynArray<Listener>& lList = m_Listeners[EntityKey(InEntity)];

        // One handler per (event, behaviour): listening again replaces it.
        for (Listener& lListener : lList)
        {
            if (lListener.EventType == InEventType && lListener.BehaviourType == InBehaviourType)
            {
                lListener.Thunk = InThunk;
                return;
            }
        }

        lList.push_back(Listener{ InEventType, InBehaviourType, InThunk });
    }

    void BehaviourSubsystem::AddBusSubscription(const EntityID InEntity, const TypeId InBehaviourType,
                                                const Uint64 InEventKey, const FBehaviourEventThunk InThunk)
    {
        EventBus& lBus = m_Context->Events.GetEventBus();

        const DelegateHandle lHandle = lBus.SubscribeErased(InEventKey, this,
            [this, InEntity, InBehaviourType, InThunk](const void* InPayload)
            {
                if (Behaviour* lBehaviour = Resolve(InEntity, InBehaviourType))
                {
                    InThunk(*lBehaviour, InPayload);
                }
            });

        m_Subscriptions.push_back(BusSubscription{ InEntity, InBehaviourType, lHandle });
    }

    // =========================================================================
    // Timers
    // =========================================================================
    TimerHandle BehaviourSubsystem::AddTimer(const EntityID InEntity, const TypeId InBehaviourType, const float InSeconds,
                                             const bool bInRepeat, const FBehaviourTimerThunk InThunk,
                                             TFunction<void()> InCallback)
    {
        if (m_bShutdown)
        {
            return TimerHandle{};
        }

        Timer lTimer;
        lTimer.Id            = m_NextTimerId++;
        lTimer.Entity        = InEntity;
        lTimer.BehaviourType = InBehaviourType;
        lTimer.Interval      = std::max(InSeconds, 0.f);
        lTimer.Due           = m_Time + lTimer.Interval;
        lTimer.bRepeat       = bInRepeat;
        lTimer.Thunk         = InThunk;
        lTimer.Callback      = Move(InCallback);

        m_Timers.push_back(Move(lTimer));
        return TimerHandle{ m_Timers.back().Id };
    }

    void BehaviourSubsystem::ClearTimer(const TimerHandle InHandle)
    {
        if (!InHandle.IsValid())
        {
            return;
        }

        for (Timer& lTimer : m_Timers)
        {
            if (lTimer.Id == InHandle.Id)
            {
                lTimer.Id = 0;
                return;
            }
        }
    }

    void BehaviourSubsystem::TickTimers()
    {
        // Timers added by a handler wait for the next frame.
        const Uint64 lCount = m_Timers.size();
        for (Uint64 lIndex = 0; lIndex < lCount; ++lIndex)
        {
            if (m_Timers[lIndex].Id == 0 || m_Timers[lIndex].Due > m_Time)
            {
                continue;
            }

            const EntityID lOwner = m_Timers[lIndex].Entity;
            Behaviour*     lSelf  = Resolve(lOwner, m_Timers[lIndex].BehaviourType);

            if (lSelf == nullptr || IsDestroyPending(lOwner))
            {
                m_Timers[lIndex].Id = 0;
                continue;
            }

            if (m_Timers[lIndex].bRepeat)
            {
                // A long frame fires once, not several times.
                m_Timers[lIndex].Due = std::max(m_Timers[lIndex].Due + m_Timers[lIndex].Interval, m_Time);
            }
            else
            {
                m_Timers[lIndex].Id = 0;
            }

            // Copied: the handler may add timers (the vector may grow).
            const FBehaviourTimerThunk lThunk    = m_Timers[lIndex].Thunk;
            const TFunction<void()>    lCallback = m_Timers[lIndex].Callback;

            if (lThunk != nullptr) { lThunk(*lSelf); }
            else if (lCallback)    { lCallback(); }
        }

        m_Timers.erase(std::remove_if(m_Timers.begin(), m_Timers.end(),
                                      [](const Timer& InTimer) { return InTimer.Id == 0; }),
                       m_Timers.end());
    }

    // =========================================================================
    // Queries
    // =========================================================================
    BehaviourSubsystem::TypeBucket* BehaviourSubsystem::FindBucket(const TypeId InType)
    {
        for (TypeBucket& lBucket : m_Buckets)
        {
            if (lBucket.Type == InType) { return &lBucket; }
        }
        return nullptr;
    }

    const BehaviourSubsystem::TypeBucket* BehaviourSubsystem::FindBucket(const TypeId InType) const
    {
        for (const TypeBucket& lBucket : m_Buckets)
        {
            if (lBucket.Type == InType) { return &lBucket; }
        }
        return nullptr;
    }

    Behaviour* BehaviourSubsystem::Resolve(const EntityID InEntity, const TypeId InBehaviourType) const
    {
        const TypeBucket* lBucket = FindBucket(InBehaviourType);
        if (lBucket == nullptr || !m_Context->OwningWorld.IsValid(InEntity))
        {
            return nullptr;
        }

        Behaviour* lBehaviour = lBucket->Entry->TryGetBehaviour(m_Context->OwningWorld.GetRegistry(), InEntity);
        return (lBehaviour != nullptr && lBehaviour->IsStarted()) ? lBehaviour : nullptr;
    }

    Uint64 BehaviourSubsystem::GetStartedCount() const noexcept
    {
        Uint64 lCount = 0;
        for (const TypeBucket& lBucket : m_Buckets)
        {
            for (const Instance& lInstance : lBucket.Instances)
            {
                if (lInstance.Ptr != nullptr && lInstance.Ptr->IsStarted()) { ++lCount; }
            }
        }
        return lCount;
    }

    Uint64 BehaviourSubsystem::GetTimerCount() const noexcept
    {
        return static_cast<Uint64>(std::count_if(m_Timers.begin(), m_Timers.end(),
                                                 [](const Timer& InTimer) { return InTimer.Id != 0; }));
    }

    // =========================================================================
    // Physics -> entity events
    // =========================================================================
    void BehaviourSubsystem::BindPhysicsEvents()
    {
        EventBus& lBus = m_Context->Events.GetEventBus();
        lBus.Subscribe<PhysicsCollisionBegan>(this, &BehaviourSubsystem::OnPhysicsCollisionBegan);
        lBus.Subscribe<PhysicsCollisionEnded>(this, &BehaviourSubsystem::OnPhysicsCollisionEnded);
        lBus.Subscribe<PhysicsOverlapBegan>(this, &BehaviourSubsystem::OnPhysicsOverlapBegan);
        lBus.Subscribe<PhysicsOverlapEnded>(this, &BehaviourSubsystem::OnPhysicsOverlapEnded);
        lBus.Subscribe<PhysicsExitedWorldBounds>(this, &BehaviourSubsystem::OnPhysicsExitedWorldBounds);
    }

    void BehaviourSubsystem::OnPhysicsCollisionBegan(const PhysicsCollisionBegan& InEvent)
    {
        if (InEvent.SourceWorld != &m_Context->OwningWorld) { return; }

        World& lWorld = m_Context->OwningWorld;
        Send(InEvent.EntityA, CollisionBegan{ Entity(InEvent.EntityB, &lWorld) });
        Send(InEvent.EntityB, CollisionBegan{ Entity(InEvent.EntityA, &lWorld) });
    }

    void BehaviourSubsystem::OnPhysicsCollisionEnded(const PhysicsCollisionEnded& InEvent)
    {
        if (InEvent.SourceWorld != &m_Context->OwningWorld) { return; }

        World& lWorld = m_Context->OwningWorld;
        Send(InEvent.EntityA, CollisionEnded{ Entity(InEvent.EntityB, &lWorld) });
        Send(InEvent.EntityB, CollisionEnded{ Entity(InEvent.EntityA, &lWorld) });
    }

    void BehaviourSubsystem::OnPhysicsOverlapBegan(const PhysicsOverlapBegan& InEvent)
    {
        if (InEvent.SourceWorld != &m_Context->OwningWorld) { return; }

        World& lWorld = m_Context->OwningWorld;
        Send(InEvent.OverlapEntity, OverlapBegan{ Entity(InEvent.OtherEntity, &lWorld), /*bIsSensor*/true });
        Send(InEvent.OtherEntity, OverlapBegan{ Entity(InEvent.OverlapEntity, &lWorld), /*bIsSensor*/false });
    }

    void BehaviourSubsystem::OnPhysicsOverlapEnded(const PhysicsOverlapEnded& InEvent)
    {
        if (InEvent.SourceWorld != &m_Context->OwningWorld) { return; }

        World& lWorld = m_Context->OwningWorld;
        Send(InEvent.OverlapEntity, OverlapEnded{ Entity(InEvent.OtherEntity, &lWorld), /*bIsSensor*/true });
        Send(InEvent.OtherEntity, OverlapEnded{ Entity(InEvent.OverlapEntity, &lWorld), /*bIsSensor*/false });
    }

    void BehaviourSubsystem::OnPhysicsExitedWorldBounds(const PhysicsExitedWorldBounds& InEvent)
    {
        if (InEvent.SourceWorld != &m_Context->OwningWorld) { return; }

        Send(InEvent.Entity, ExitedWorldBounds{ InEvent.LastPosition });
    }
}
