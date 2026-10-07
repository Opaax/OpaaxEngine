#pragma once

#include "Core/Events/DelegateHandle.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class ResourcePrefabResolver;
    struct WorldContext;
    struct PhysicsCollisionBegan;
    struct PhysicsCollisionEnded;
    struct PhysicsOverlapBegan;
    struct PhysicsOverlapEnded;
    struct PhysicsExitedWorldBounds;

    // =============================================================================
    // BehaviourSubsystem — runs a Play world's behaviours: starts them, updates them, delivers their
    //   events and timers, destroys them. One per Play world (Edit worlds run no gameplay).
    //
    //   Start points: before each pass, after each behaviour type's pass, at the end of a spawn, and
    //   before an event is delivered. A behaviour added during a pass is first updated in the next one.
    //   Destruction asked by gameplay code waits for the end of the pass. Every started behaviour gets
    //   OnDestroy exactly once, while its entity still has all its components, including when the
    //   world ends.
    // =============================================================================
    class BehaviourSubsystem final : public WorldSubsystemBase, private IBehaviourSignalSink
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(BehaviourSubsystem)

        /** Play worlds only. */
        static bool ShouldCreate(const World& InWorld);

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        explicit BehaviourSubsystem(WorldContext& InContext);
        ~BehaviourSubsystem() override;

        BehaviourSubsystem(const BehaviourSubsystem&)            = delete;
        BehaviourSubsystem& operator=(const BehaviourSubsystem&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Lifecycle
        // =============================================================================
    public:
        /** Starts every behaviour waiting for OnStart (in the order they were added). */
        void StartPending();

        /** Destroys InEntity and its children at the end of the current pass. */
        void DestroyLater(EntityID InEntity);

        /** Removes one behaviour from InEntity at the end of the current pass. */
        void RemoveBehaviourLater(EntityID InEntity, TypeId InBehaviourType);

        /** Runs the queued destructions now (between passes). */
        void FlushDestroyed();

        /** True when InEntity is queued for destruction. */
        bool IsDestroyPending(EntityID InEntity) const;

        /** True once the world is ending: its behaviours are being destroyed for good. */
        bool IsEnding() const noexcept { return m_bShutdown; }

        // =============================================================================
        // Spawning
        // =============================================================================
    public:
        /**
         * Instantiates a prefab (runtime-spawned: never saved) with its root at InPosition, then starts
         * its behaviours before returning.
         * @return The root entity, or an invalid Entity if the prefab cannot be loaded (logged) or the
         *         world is ending
         */
        Entity SpawnPrefab(const OpaaxString& InPrefabPath, const Vector2F& InPosition, float InRotationDegrees = 0.f);

        // =============================================================================
        // Entity events
        // =============================================================================
    public:
        /** Delivers InEvent to the behaviours on InTarget listening for E (then its parents for a bubbling E). */
        template<typename E>
        void Send(EntityID InTarget, const E& InEvent)
        {
            SendErased(InTarget, TypeIdOf<E>(), &InEvent, CBubblingEvent<E>);
        }

        void SendErased(EntityID InTarget, TypeId InEventType, const void* InEvent, bool bInBubbles);

        /** InEntity's InBehaviourType receives InEventType through InThunk until it ends. */
        void AddListener(EntityID InEntity, TypeId InEventType, TypeId InBehaviourType, FBehaviourEventThunk InThunk);

        /** Inside a handler: the bubbling event being delivered climbs no further (this entity's other listeners still get it). */
        void StopPropagation() noexcept { m_bStopPropagation = true; }

        /** Subscribes InEntity's InBehaviourType to an engine EventBus event until it ends. */
        void AddBusSubscription(EntityID InEntity, TypeId InBehaviourType, Uint64 InEventKey, FBehaviourEventThunk InThunk);

        // =============================================================================
        // Timers
        // =============================================================================
    public:
        /**
         * A timer owned by InEntity's InBehaviourType: InThunk if set, otherwise InCallback. It is due
         * InSeconds of world time after now, wherever in the frame it is set.
         */
        TimerHandle AddTimer(EntityID InEntity, TypeId InBehaviourType, float InSeconds, bool bInRepeat,
                             FBehaviourTimerThunk InThunk, TFunction<void()> InCallback);

        void ClearTimer(TimerHandle InHandle);

        // =============================================================================
        // Queries
        // =============================================================================
    public:
        /** InEntity's started behaviour of InBehaviourType, or null. */
        Behaviour* Resolve(EntityID InEntity, TypeId InBehaviourType) const;

        /** Behaviours started and not ended. */
        Uint64 GetStartedCount() const noexcept;

        /** Behaviours waiting for OnStart. */
        Uint64 GetPendingCount() const noexcept { return static_cast<Uint64>(m_Pending.size()); }

        Uint64 GetTimerCount() const noexcept;

        /** Seconds this world has been updated. */
        double GetTime() const noexcept { return m_Time; }

        /** The current frame's delta time. */
        float GetDeltaTime() const noexcept { return m_DeltaTime; }

        WorldContext& GetContext() const noexcept { return *m_Context; }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        //~Begin IBehaviourSignalSink interface
        void OnBehaviourConstructed(EntityID InEntity, TypeId InType) override;
        void OnBehaviourDestroyed(EntityID InEntity, TypeId InType, Behaviour& InBehaviour) override;
        //~End IBehaviourSignalSink interface

        struct Instance
        {
            EntityID   Entity    = ENTITY_NONE;
            Behaviour* Ptr       = nullptr;   // null once ended
            Uint64     StartPass = 0;
        };

        struct TypeBucket
        {
            TypeId                 Type  = 0;
            const IComponentEntry* Entry = nullptr;
            TDynArray<Instance>    Instances;
        };

        struct PendingStart
        {
            EntityID Entity = ENTITY_NONE;
            TypeId   Type   = 0;
        };

        struct PendingRemoval
        {
            EntityID Entity = ENTITY_NONE;
            TypeId   Type   = 0;
        };

        struct Listener
        {
            TypeId               EventType     = 0;
            TypeId               BehaviourType = 0;
            FBehaviourEventThunk Thunk         = nullptr;
        };

        struct BusSubscription
        {
            EntityID       Entity        = ENTITY_NONE;
            TypeId         BehaviourType = 0;
            DelegateHandle Handle;
        };

        struct Timer
        {
            Uint64               Id            = 0;   // 0 once cleared
            EntityID             Entity        = ENTITY_NONE;
            TypeId               BehaviourType = 0;
            double               Due           = 0.0;   // the world time it fires at
            float                Interval      = 0.f;
            bool                 bRepeat       = false;
            FBehaviourTimerThunk Thunk         = nullptr;
            TFunction<void()>    Callback;
        };

        TypeBucket* FindBucket(TypeId InType);
        const TypeBucket* FindBucket(TypeId InType) const;

        /** Binds and starts one pending behaviour, unless it is gone or already started. */
        void StartOne(PendingStart InPending);

        /** Starts InEntity's pending behaviours now, ahead of the others (an event is about to reach them). */
        void StartPendingOn(EntityID InEntity);

        /** One update or fixed-update pass over every behaviour type, in registration order. */
        void RunPass(bool bInFixed, float InDeltaTime);

        void DeliverTo(EntityID InTarget, TypeId InEventType, const void* InEvent);

        /** The world is about to destroy InEntity: its behaviours end while it is whole. */
        void OnEntityDestroying(EntityID InEntity);

        /** Ends InEntity's behaviour of the bucket's type, if it has started and not ended yet. */
        void EndIfStarted(EntityID InEntity, const TypeBucket& InBucket);

        /** OnDestroy (gameplay calls still work in it), then forgets everything the behaviour registered. */
        void EndBehaviour(EntityID InEntity, TypeId InType, Behaviour& InBehaviour);

        /** Drops the behaviour's listeners, subscriptions and timers. */
        void ForgetBehaviour(EntityID InEntity, TypeId InType);

        /** Drops ended instances from the buckets (never during a pass). */
        void CompactBuckets();

        /** Fires the timers due by now. Once per frame, after the update pass. */
        void TickTimers();

        void BindPhysicsEvents();
        void OnPhysicsCollisionBegan(const PhysicsCollisionBegan& InEvent);
        void OnPhysicsCollisionEnded(const PhysicsCollisionEnded& InEvent);
        void OnPhysicsOverlapBegan(const PhysicsOverlapBegan& InEvent);
        void OnPhysicsOverlapEnded(const PhysicsOverlapEnded& InEvent);
        void OnPhysicsExitedWorldBounds(const PhysicsExitedWorldBounds& InEvent);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // owned by the World

        TDynArray<TypeBucket>     m_Buckets;   // one per behaviour type, registration order
        TDynArray<PendingStart>   m_Pending;
        TDynArray<EntityID>       m_DestroyQueue;
        TUnorderedSet<Uint32>     m_DestroyPending;
        TDynArray<PendingRemoval> m_RemoveQueue;

        TUnorderedMap<Uint32, TDynArray<Listener>> m_Listeners;   // keyed by entity
        TDynArray<BusSubscription>                 m_Subscriptions;
        TDynArray<Timer>                           m_Timers;

        /** Kept for the world's life, so prefabs spawned often stay loaded. */
        TUniquePtr<ResourcePrefabResolver> m_PrefabResolver;

        DelegateHandle m_EntityDestroyingHandle;

        Uint64 m_PassIndex    = 0;
        Uint64 m_NextTimerId  = 1;
        double m_Time         = 0.0;
        float  m_DeltaTime    = 0.f;
        Uint32 m_SendDepth    = 0;

        bool m_bStopPropagation  = false;
        bool m_bStarting         = false;
        bool m_bShutdown         = false;
        bool m_bSignalsConnected = false;
        bool m_bWarnedSendDepth  = false;
    };
}
