#pragma once

#include <concepts>
#include <type_traits>

#include <nlohmann/json.hpp>

#include "Audio/AudioTypes.h"
#include "Core/EngineAPI.h"
#include "Core/Events/EventBus.h"
#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Reflection/TypeInfo.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/Reflection/PropertyJson.h"
#include "Input/InputCodes.h"
#include "Input/Mapping/InputTypes.h"
#include "World/Components/TransformComponent.h"   // GetTransform(): every behaviour moves something
#include "World/Entity/Entity.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A behaviour is gameplay logic on an entity: a class with its own fields (saved like a
// component's, from OPAAX_PROPERTIES) and lifecycle hooks. One of each type per entity.
//
//   class Coin : public Opaax::Behaviour
//   {
//   public:
//       Opaax::Int32 Value = 1;
//       OPAAX_PROPERTIES(Coin, OPAAX_PROP(Value))
//
//       void OnStart() override { Listen<&Coin::OnOverlap>(); }
//       void OnOverlap(const Opaax::OverlapBegan& InEvent) { Destroy(); }
//   };
//   OPAAX_REGISTER_BEHAVIOUR(Coin);
//
// Lifecycle (Play worlds only), the same whatever created the entity (map load, Play copy,
// Spawn, code):
//   OnStart       once, after the entity's whole batch exists, before any update or event
//   OnUpdate      every frame          OnFixedUpdate  every fixed step, before physics
//   OnDestroy     once, for every started behaviour, whatever ended it. The entity still has all
//                 its components, and the behaviour can still spawn, send and broadcast
//                 (IsWorldEnding() tells the end of the level from a gameplay destroy).
// Destroying an entity from gameplay code (Destroy) is deferred to the end of the current pass.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    class Behaviour;
    class BehaviourSubsystem;
    class DebugDraw;
    class InputManager;
    struct InputActionState;

    inline constexpr LogCategory LogBehaviour{"Behaviour"};

    /** Calls a behaviour's event handler with an untyped event. */
    using FBehaviourEventThunk = void (*)(Behaviour&, const void*);

    /** Calls a behaviour's timer handler. */
    using FBehaviourTimerThunk = void (*)(Behaviour&);

    /**
     * An entity event that climbs the parent chain after its target, until a handler calls
     * StopPropagation(). Opt in with `static constexpr bool Bubbles = true;` in the event struct.
     */
    template<typename E>
    concept CBubblingEvent = requires { { E::Bubbles } -> std::convertible_to<bool>; } && E::Bubbles;

    /** Identifies a timer, for ClearTimer. Zero is no timer. */
    struct TimerHandle
    {
        Uint64 Id = 0;

        bool IsValid() const noexcept { return Id != 0; }
    };

    /** What a RayCast hit. False when it hit nothing. */
    struct RayHit
    {
        Entity   Target;                    // the entity whose collider was hit
        Vector2F Point    = { 0.f, 0.f };   // where, in world units
        Vector2F Normal   = { 0.f, 0.f };   // the surface's normal there
        float    Fraction = 0.f;            // how far along the ray, 0 to 1

        explicit operator bool() const noexcept { return Target.IsValid(); }
    };

    namespace BehaviourDetail
    {
        /** The class and event type of a handler `void (C::*)(const E&)`. */
        template<typename TMethod>
        struct TEventMethodTraits;

        template<typename C, typename E>
        struct TEventMethodTraits<void (C::*)(const E&)>
        {
            using Class = C;
            using Event = E;
        };

        /** A const handler is a handler too. */
        template<typename C, typename E>
        struct TEventMethodTraits<void (C::*)(const E&) const> : TEventMethodTraits<void (C::*)(const E&)> {};

        /** The class of a handler `void (C::*)()`. */
        template<typename TMethod>
        struct TTimerMethodTraits;

        template<typename C>
        struct TTimerMethodTraits<void (C::*)()>
        {
            using Class = C;
        };

        template<typename C>
        struct TTimerMethodTraits<void (C::*)() const> : TTimerMethodTraits<void (C::*)()> {};

        template<typename C, typename E, auto Method>
        void CallEventHandler(Behaviour& InSelf, const void* InEvent)
        {
            (static_cast<C&>(InSelf).*Method)(*static_cast<const E*>(InEvent));
        }

        template<typename C, auto Method>
        void CallTimerHandler(Behaviour& InSelf)
        {
            (static_cast<C&>(InSelf).*Method)();
        }
    }

    // =============================================================================
    // Behaviour — the base of every gameplay behaviour. See the usage notes above.
    //   Stored like a component (entt): instances never move, so `this` stays valid for the
    //   behaviour's whole life. Copies carry the fields, never the runtime binding.
    // =============================================================================
    class Behaviour
    {
        // =============================================================================
        // Storage
        // =============================================================================
    public:
        /** Instances never move in storage, so a `this` held elsewhere stays valid. */
        static constexpr bool in_place_delete = true;

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        virtual ~Behaviour() = default;

    protected:
        // Protected: a behaviour cannot be copied or moved as a plain Behaviour (no slicing).
        Behaviour() = default;
        Behaviour(const Behaviour&) noexcept {}
        Behaviour(Behaviour&&) noexcept {}
        Behaviour& operator=(const Behaviour&) noexcept { return *this; }
        Behaviour& operator=(Behaviour&&) noexcept { return *this; }

        // =============================================================================
        // Lifecycle
        // =============================================================================
    public:
        /** Once, before the first update and the first event. */
        virtual void OnStart() {}

        /** Every frame. */
        virtual void OnUpdate(float /*InDeltaTime*/) {}

        /** Every fixed step, before physics. */
        virtual void OnFixedUpdate(float /*InFixedDeltaTime*/) {}

        /** Once, when the behaviour or its entity ends, the entity still whole. Only if OnStart ran. */
        virtual void OnDestroy() {}

        // =============================================================================
        // Entity
        // =============================================================================
    public:
        /** The entity this behaviour is on. Invalid before OnStart. */
        Entity GetEntity() const noexcept { return Entity(m_Entity, m_World); }

        /** The world the entity lives in. Valid from OnStart to OnDestroy. */
        World& GetWorld() const;

        /** True between OnStart and OnDestroy. */
        bool IsStarted() const noexcept { return m_bStarted && !m_bEnded; }

        /** The entity's name. */
        const OpaaxString& GetEntityName() const;

        /** A component (or behaviour) on this entity, or null. */
        template<typename T>
        T* TryGet() const
        {
            return (m_World != nullptr) ? m_World->GetRegistry().try_get<T>(m_Entity) : nullptr;
        }

        /** A component on this entity that must exist. */
        template<typename T>
        T& Get() const { return m_World->GetRegistry().get<T>(m_Entity); }

        template<typename T>
        bool Has() const { return m_World != nullptr && m_World->GetRegistry().all_of<T>(m_Entity); }

        /** Adds a component (or behaviour) to this entity. A behaviour starts at the next start point. */
        template<typename T, typename... TArgs>
        T& Add(TArgs&&... InArgs)
        {
            return m_World->GetRegistry().get_or_emplace<T>(m_Entity, Forward<TArgs>(InArgs)...);
        }

        /** Removes a component; a behaviour is removed at the end of the current pass. */
        template<typename T>
        void Remove()
        {
            if constexpr (std::derived_from<T, Behaviour>) { RemoveBehaviourLater(TypeIdOf<T>()); }
            else                                           { m_World->GetRegistry().remove<T>(m_Entity); }
        }

        // =============================================================================
        // Transform (local to the parent; the World variants compose the parent chain)
        // =============================================================================
    public:
        TransformComponent& GetTransform() const;

        Vector2F GetPosition() const;
        void     SetPosition(const Vector2F& InPosition);

        /** Degrees, counter-clockwise. */
        float GetRotation() const;
        void  SetRotation(float InDegrees);

        Vector2F GetWorldPosition() const;
        void     SetWorldPosition(const Vector2F& InPosition);

        // =============================================================================
        // Creating and destroying
        // =============================================================================
    public:
        /** Destroys this entity (and its children) at the end of the current pass. */
        void Destroy();

        /** Destroys InEntity (and its children) at the end of the current pass. */
        void Destroy(Entity InEntity) const;

        /** Destroys this entity after InSeconds. */
        void DestroyAfter(float InSeconds);

        /** A new empty entity (runtime-spawned: never saved). */
        Entity CreateEntity(const OpaaxString& InName = OpaaxString("Entity")) const;

        /**
         * An instance of a prefab, its root at InPosition. Its behaviours have started when this
         * returns, so the caller can Send to it on the next line.
         * @param InPrefabPath Asset-relative ("Prefabs/Bullet.opaaxprefab")
         * @return The root entity, or an invalid Entity if the prefab cannot be loaded
         */
        Entity Spawn(const OpaaxString& InPrefabPath, const Vector2F& InPosition, float InRotationDegrees = 0.f) const;

        // =============================================================================
        // Finding
        // =============================================================================
    public:
        /** The first entity named InName, or an invalid Entity. */
        Entity FindEntity(const OpaaxString& InName) const;

        /** The first started behaviour of type T in the world, or null. */
        template<typename T>
        T* FindBehaviour() const
        {
            T* lFound = nullptr;
            for (const auto [lEntity, lBehaviour] : m_World->GetRegistry().view<T>().each())
            {
                if (lBehaviour.IsStarted()) { lFound = &lBehaviour; break; }
            }
            return lFound;
        }

        /** Calls InFunc(T&) for every started behaviour of type T in the world. */
        template<typename T, typename TFunc>
        void ForEachBehaviour(TFunc&& InFunc) const
        {
            for (const auto [lEntity, lBehaviour] : m_World->GetRegistry().view<T>().each())
            {
                if (lBehaviour.IsStarted()) { InFunc(lBehaviour); }
            }
        }

        /** The parent entity, or an invalid Entity for a root. */
        Entity GetParent() const;

        /** Attaches this entity under InParent (invalid detaches). Keeps the world pose by default. */
        void SetParent(Entity InParent, bool bInKeepWorldPose = true);

        // =============================================================================
        // Entity events — targeted, typed, delivered immediately
        // =============================================================================
    public:
        /** Delivers InEvent to the behaviours on InTarget that Listen for E. */
        template<typename E>
        void Send(Entity InTarget, const E& InEvent) const
        {
            SendErased(InTarget.GetHandle(), TypeIdOf<E>(), &InEvent, CBubblingEvent<E>);
        }

        /**
         * Starts receiving events of the handler's type sent to this entity, until OnDestroy:
         *   Listen<&Door::OnOpen>();   // void Door::OnOpen(const OpenDoor&)
         */
        template<auto Method>
        void Listen()
        {
            using Traits = BehaviourDetail::TEventMethodTraits<decltype(Method)>;
            using Class  = typename Traits::Class;
            using Event  = typename Traits::Event;
            static_assert(std::derived_from<Class, Behaviour>, "Listen: the handler must be a behaviour's member.");

            ListenErased(TypeIdOf<Event>(), TypeIdOf<Class>(), &BehaviourDetail::CallEventHandler<Class, Event, Method>);
        }

        /** Inside a handler of a bubbling event: the event goes no further up the parent chain. */
        void StopPropagation() const;

        // =============================================================================
        // Global events — the engine EventBus, for "whoever cares"
        // =============================================================================
    public:
        /** Publishes InEvent on the engine EventBus, now. */
        template<typename E>
        void Broadcast(const E& InEvent) const
        {
            static_assert(std::is_trivially_copyable_v<E>, "EventBus payloads must be trivially-copyable structs");
            BroadcastErased(Detail::EventTypeKey<E>(), &InEvent);
        }

        /**
         * Receives every E published on the engine EventBus until OnDestroy:
         *   Subscribe<&Hud::OnScore>();   // void Hud::OnScore(const ScoreChanged&)
         */
        template<auto Method>
        void Subscribe()
        {
            using Traits = BehaviourDetail::TEventMethodTraits<decltype(Method)>;
            using Class  = typename Traits::Class;
            using Event  = typename Traits::Event;
            static_assert(std::derived_from<Class, Behaviour>, "Subscribe: the handler must be a behaviour's member.");
            static_assert(std::is_trivially_copyable_v<Event>, "EventBus payloads must be trivially-copyable structs");

            SubscribeErased(Detail::EventTypeKey<Event>(), TypeIdOf<Class>(),
                            &BehaviourDetail::CallEventHandler<Class, Event, Method>);
        }

        // =============================================================================
        // Timers — world time, checked once per frame after the updates; cleared at OnDestroy
        // =============================================================================
    public:
        /** Calls Method after InSeconds (every InSeconds if bInRepeat): SetTimer<&Turret::Fire>(0.5f, true). */
        template<auto Method>
        TimerHandle SetTimer(float InSeconds, bool bInRepeat = false)
        {
            using Class = typename BehaviourDetail::TTimerMethodTraits<decltype(Method)>::Class;
            static_assert(std::derived_from<Class, Behaviour>, "SetTimer: the handler must be a behaviour's member.");

            return SetTimerErased(InSeconds, bInRepeat, TypeIdOf<Class>(),
                                  &BehaviourDetail::CallTimerHandler<Class, Method>, {});
        }

        /** Calls InCallback after InSeconds (every InSeconds if bInRepeat). */
        TimerHandle SetTimer(float InSeconds, TFunction<void()> InCallback, bool bInRepeat = false);

        void ClearTimer(TimerHandle InHandle);

        // =============================================================================
        // Physics — this entity's dynamic body (a Collider and a Dynamic Rigidbody). Without one
        //   the setters do nothing (with a warning, once) and the getters return zero. A body
        //   spawned this frame can be launched at once. Moving the Transform teleports the body.
        // =============================================================================
    public:
        /** World units per second. */
        Vector2F GetVelocity() const;
        void     SetVelocity(const Vector2F& InVelocity);

        /** Degrees per second, counter-clockwise. */
        float GetAngularVelocity() const;
        void  SetAngularVelocity(float InDegreesPerSecond);

        /** Over the next fixed step (mass * units / s^2): call it every OnFixedUpdate for a steady push. */
        void AddForce(const Vector2F& InForce);

        /** An instant kick (mass * units / s): a jump, a hit. */
        void AddImpulse(const Vector2F& InImpulse);

        /**
         * Turns the body counter-clockwise over the next fixed step (mass * units^2 / s^2). How fast
         * it turns depends on its rotational inertia: for a box, mass * (width^2 + height^2) / 12.
         */
        void AddTorque(float InTorque);

        /** An instant counter-clockwise turn (mass * units^2 / s): the turn rate changes by impulse / inertia. */
        void AddAngularImpulse(float InImpulse);

        /** From the collider's density and area. */
        float GetMass() const;

        // =============================================================================
        // Physics queries — any collider in the world; nothing is found in a world without physics.
        //   InChannelMask: the channels to find, CategoryBit(ECollisionChannel::WorldStatic) | ...
        //   (Physics/Collision/CollisionChannel.h); every channel by default.
        // =============================================================================
    public:
        /**
         * The closest collider along a ray from InOrigin towards InDirection, up to InDistance world
         * units. A collider the ray starts inside is not hit, so a ray from the entity's own position
         * skips its own collider:
         *   if (const RayHit lGround = RayCast(GetWorldPosition(), { 0.f, -1.f }, 40.f)) { ... }
         */
        RayHit RayCast(const Vector2F& InOrigin, const Vector2F& InDirection, float InDistance,
                       Uint64 InChannelMask = ~0ull) const;

        /**
         * Every entity whose collider's bounding box overlaps the world box [InMin, InMax] (a round or
         * rotated collider near a corner can be listed without touching it). OutEntities is cleared first.
         */
        void OverlapBox(const Vector2F& InMin, const Vector2F& InMax, TDynArray<Entity>& OutEntities,
                        Uint64 InChannelMask = ~0ull) const;

        // =============================================================================
        // Audio — the world's sounds (they stop when the world ends). Silent without audio.
        // =============================================================================
    public:
        /** Plays a clip, not positioned (music, UI, a pickup): PlaySound("Audio/Coin.wav"). */
        SoundHandle PlaySound(const OpaaxString& InClipPath, float InVolume = 1.f, float InPitch = 1.f) const;

        /** Plays a clip at a place in the world, heard from the listener (panned, attenuated). */
        SoundHandle PlaySoundAt(const OpaaxString& InClipPath, const Vector2F& InPosition, float InVolume = 1.f) const;

        void StopSound(SoundHandle InSound) const;

        /** Plays this entity's AudioSourceComponent from the start. */
        void PlayAudioSource() const;
        void StopAudioSource() const;

        // =============================================================================
        // Input
        // =============================================================================
    public:
        const InputManager& GetInput() const;

        bool IsKeyDown(EKeyCode InKey) const;

        /** True on the frame the key went down. */
        bool WasKeyPressed(EKeyCode InKey) const;

        /** True on the frame the key went up. */
        bool WasKeyReleased(EKeyCode InKey) const;

        // =============================================================================
        // Input actions — the game's named actions (.opaaxinputmap). They read as zero, and
        //   BindAction does nothing, when no game is running (no input mapping).
        // =============================================================================
    public:
        /** This frame's value: AsBool(), AsAxis1D(), AsAxis2D() (GetAction("Move").AsAxis2D()). */
        InputActionValue GetAction(OpaaxStringID InAction) const;

        /** True every frame the action is active. */
        bool IsActionActive(OpaaxStringID InAction) const;

        /** True the frame the action started (a press). */
        bool WasActionStarted(OpaaxStringID InAction) const;

        /** True the frame the action ended (a release). */
        bool WasActionCompleted(OpaaxStringID InAction) const;

        /**
         * Calls Method when InAction fires for InTrigger, until OnDestroy. Handlers run before the
         * world updates:
         *   BindAction<&Player::OnJump>("Jump", EInputTrigger::Started);   // void Player::OnJump(const InputActionValue&)
         */
        template<auto Method>
        void BindAction(OpaaxStringID InAction, EInputTrigger InTrigger)
        {
            using Traits = BehaviourDetail::TEventMethodTraits<decltype(Method)>;
            using Class  = typename Traits::Class;
            static_assert(std::derived_from<Class, Behaviour>, "BindAction: the handler must be a behaviour's member.");
            static_assert(std::is_same_v<typename Traits::Event, InputActionValue>,
                          "BindAction: the handler takes a const InputActionValue&.");

            BindActionErased(InAction, InTrigger, TypeIdOf<Class>(),
                             &BehaviourDetail::CallEventHandler<Class, InputActionValue, Method>);
        }

        // =============================================================================
        // Time and game
        // =============================================================================
    public:
        /** In OnDestroy: true when the whole world is ending (level change, quit), not just this entity. */
        bool IsWorldEnding() const;

        /** Seconds the world has been playing. */
        double GetTime() const;

        /** This frame's delta time, in seconds. */
        float GetDeltaTime() const;

        /** Opens a level at the start of the next frame (asset-relative .opaaxlevel). */
        void OpenLevel(const OpaaxString& InLevelPath) const;

        /** Closes the game at the start of the next frame (the editor stops Play instead). */
        void QuitGame() const;

        /** Debug shapes, drawn this frame only. */
        DebugDraw& GetDebugDraw() const;

        /** The world's services (resources, paths, input, event bus, ...). */
        WorldContext& GetContext() const;

        /** One of this world's subsystems, the engine's or the game's (GetSubsystem<WaveSpawner>()), or null. */
        template<typename T>
        T* GetSubsystem() const
        {
            return (m_World != nullptr) ? m_World->GetSubsystems().template GetSubsystem<T>() : nullptr;
        }

        /**
         * One of the running game's subsystems, which last across levels (GetGameSubsystem<ScoreKeeper>()),
         * or null when no game is running.
         */
        template<typename T>
        T* GetGameSubsystem() const
        {
            const WorldContext* lContext = (m_World != nullptr) ? m_World->GetContext() : nullptr;
            GameInstance*       lGame    = (lContext != nullptr) ? lContext->Game : nullptr;
            return (lGame != nullptr) ? lGame->GetSubsystems().template GetSubsystem<T>() : nullptr;
        }

        // =============================================================================
        // Runtime binding (set by BehaviourSubsystem)
        // =============================================================================
    private:
        friend class BehaviourSubsystem;

        void SendErased(EntityID InTarget, TypeId InEventType, const void* InEvent, bool bInBubbles) const;
        void ListenErased(TypeId InEventType, TypeId InBehaviourType, FBehaviourEventThunk InThunk);
        void BroadcastErased(Uint64 InEventKey, const void* InEvent) const;
        void SubscribeErased(Uint64 InEventKey, TypeId InBehaviourType, FBehaviourEventThunk InThunk);
        void BindActionErased(OpaaxStringID InAction, EInputTrigger InTrigger, TypeId InBehaviourType,
                              FBehaviourEventThunk InThunk);

        /** This frame's state of InAction, or null (unknown action, or no input mapping). */
        const InputActionState* FindActionState(OpaaxStringID InAction) const;
        TimerHandle SetTimerErased(float InSeconds, bool bInRepeat, TypeId InBehaviourType,
                                   FBehaviourTimerThunk InThunk, TFunction<void()> InCallback);
        void RemoveBehaviourLater(TypeId InBehaviourType);

        /** The runtime this behaviour is started in. Logs and returns null before OnStart and after OnDestroy. */
        BehaviourSubsystem* RequireRuntime(const char* InWhat) const;

        /** Logs (once per behaviour) that a physics call found no dynamic body. */
        void WarnNoBody(const char* InWhat);

        EntityID            m_Entity   = ENTITY_NONE;
        World*              m_World    = nullptr;
        BehaviourSubsystem* m_Runtime  = nullptr;   // set at OnStart, cleared after OnDestroy
        TypeId              m_Type     = 0;
        bool                m_bStarted = false;
        bool                m_bEnded   = false;
        bool                m_bWarnedNoBody = false;
    };

    /**
     * A type that can be registered as a behaviour.
     */
    template<typename T>
    concept CBehaviour = std::derived_from<T, Behaviour> && std::default_initializable<T>;

    // =============================================================================
    // JSON — found for any behaviour type (its base is in Opaax). Written from its property list;
    //   a behaviour with no OPAAX_PROPERTIES saves as an empty object.
    // =============================================================================
    template<typename T>
    requires std::derived_from<T, Behaviour>
    void to_json(nlohmann::json& OutJson, const T& InBehaviour)
    {
        if constexpr (CReflected<T>) { OutJson = PropertiesToJson(InBehaviour); }
        else                         { OutJson = nlohmann::json::object(); }
    }

    /**
     * A missing field keeps its default. A wrong-typed field keeps it too, with a warning.
     * Throws if InJson is not an object (the map loader then skips the behaviour, with a warning).
     */
    template<typename T>
    requires std::derived_from<T, Behaviour>
    void from_json(const nlohmann::json& InJson, T& OutBehaviour)
    {
        if (!InJson.is_object())
        {
            throw nlohmann::json::type_error::create(302, "a behaviour's data must be an object", &InJson);
        }

        if constexpr (CReflected<T>)
        {
            TDynArray<OpaaxString> lBadFields;
            PropertiesFromJson(InJson, OutBehaviour, &lBadFields);

            for (const OpaaxString& lField : lBadFields)
            {
                OPAAX_LOG(LogBehaviour, Warn, "'{}': field '{}' has the wrong type in the file, kept its default",
                          DeriveTypeLeafName<T>(), lField.CStr());
            }
        }
    }
}
