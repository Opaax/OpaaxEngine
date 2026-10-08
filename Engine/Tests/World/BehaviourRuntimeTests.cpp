// Suite: the behaviour runtime (BehaviourSubsystem) — when the lifecycle hooks run, deferred
// destruction, entity and global events, timers, spawning, physics events and input. Every case
// runs a real Play world with its own component registry; the behaviours write what happens to a
// journal ("<entity name>:<what>") that the case reads back.
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Application/Services/IPaths.h"
#include "Core/Events/EventBus.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/EngineEvents.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/Components/ColliderComponent.h"
#include "Physics/Components/RigidbodyComponent.h"
#include "Physics/PhysicsEvents.h"
#include "Physics/PhysicsSubsystem.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Behaviour/EntityEvents.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/Prefab/PrefabData.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    std::vector<std::string>& Journal()
    {
        static std::vector<std::string> s_Journal;
        return s_Journal;
    }

    void Note(const Behaviour& InSelf, const char* InWhat)
    {
        Journal().push_back(std::string(InSelf.GetEntityName().CStr()) + ":" + InWhat);
    }

    Uint64 CountOf(const char* InEntry)
    {
        return static_cast<Uint64>(std::count(Journal().begin(), Journal().end(), std::string(InEntry)));
    }

    /** Position of the first InEntry in the journal, or -1. */
    Int64 IndexOf(const char* InEntry)
    {
        const auto lFound = std::find(Journal().begin(), Journal().end(), std::string(InEntry));
        return (lFound == Journal().end()) ? -1 : static_cast<Int64>(lFound - Journal().begin());
    }
}

// A game's namespace, as a game module would declare its behaviours.
namespace RuntimeProbes
{
    // ---- Events ------------------------------------------------------------------------------
    struct Hit          { Int32 Damage = 0; };
    struct Ping         {};
    struct Clicked      { static constexpr bool Bubbles = true; Int32 Id = 0; };
    struct ScoreChanged { Int32 Score = 0; };

    // ---- A data component, to see whether an entity is still whole -----------------------------
    struct Marker
    {
        Int32 Value = 0;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Marker, Value)
    };

    // ---- Behaviours --------------------------------------------------------------------------

    /** Writes its lifecycle to the journal. */
    struct Probe final : Behaviour
    {
        bool DestroySelfOnUpdate = false;

        void OnStart() override { Note(*this, "Start"); }

        void OnUpdate(float) override
        {
            Note(*this, "Update");
            if (DestroySelfOnUpdate) { Destroy(); }
        }

        void OnFixedUpdate(float) override { Note(*this, "Fixed"); }

        void OnDestroy() override
        {
            Note(*this, "Destroy");
            if (Has<TransformComponent>() && Has<Marker>()) { Note(*this, "Whole"); }
            if (IsWorldEnding())                            { Note(*this, "WorldEnding"); }
        }
    };

    /** Adds an entity with a Probe during its first update. */
    struct Adder final : Behaviour
    {
        bool bAdded = false;

        void OnUpdate(float) override
        {
            if (!bAdded)
            {
                bAdded = true;
                CreateEntity(OpaaxString("Late")).Add<Probe>();
            }
        }
    };

    /** Destroys the entity named Target during each update. */
    struct Killer final : Behaviour
    {
        OpaaxString Target;

        void OnUpdate(float) override
        {
            if (const Entity lTarget = FindEntity(Target); lTarget.IsValid()) { Destroy(lTarget); }
        }
    };

    /** Removes its entity's Probe during each update. */
    struct Remover final : Behaviour
    {
        void OnUpdate(float) override { Remove<Probe>(); }
    };

    struct Ticker final : Behaviour
    {
        Int32 Updates = 0;

        void OnUpdate(float) override { ++Updates; }
    };

    struct HitCounter final : Behaviour
    {
        Int32 Hits  = 0;
        Int32 Total = 0;

        void OnStart() override
        {
            Note(*this, "Start");
            Listen<&HitCounter::OnHit>();
            Listen<&HitCounter::OnHit>();   // again: still one handler
        }

        void OnHit(const Hit& InHit)
        {
            Note(*this, "Hit");
            ++Hits;
            Total += InHit.Damage;
        }
    };

    /** A second listener type for Hit, on the same entity as a HitCounter. */
    struct Armor final : Behaviour
    {
        Int32 Hits = 0;

        void OnStart() override { Listen<&Armor::OnHit>(); }
        void OnHit(const Hit&) { ++Hits; }
    };

    /** Sends one Hit to Target during its first update. */
    struct Shooter final : Behaviour
    {
        OpaaxString Target;
        bool        bFired = false;

        void OnUpdate(float) override
        {
            if (!bFired)
            {
                bFired = true;
                Send(FindEntity(Target), Hit{ 5 });
            }
        }
    };

    /** Sends a Hit to Target from its OnStart. */
    struct Greeter final : Behaviour
    {
        OpaaxString Target;

        void OnStart() override
        {
            Note(*this, "Start");
            Send(FindEntity(Target), Hit{ 1 });
        }
    };

    struct ClickCounter final : Behaviour
    {
        Int32 Clicks = 0;
        bool  bStop  = false;

        void OnStart() override { Listen<&ClickCounter::OnClicked>(); }

        void OnClicked(const Clicked&)
        {
            ++Clicks;
            if (bStop) { StopPropagation(); }
        }
    };

    struct ClickWitness final : Behaviour
    {
        Int32 Clicks = 0;

        void OnStart() override { Listen<&ClickWitness::OnClicked>(); }
        void OnClicked(const Clicked&) { ++Clicks; }
    };

    /** Answers every Ping by sending one to Partner. */
    struct Echo final : Behaviour
    {
        OpaaxString Partner;
        Int32       Pings = 0;

        void OnStart() override { Listen<&Echo::OnPing>(); }

        void OnPing(const Ping&)
        {
            ++Pings;
            Send(FindEntity(Partner), Ping{});
        }
    };

    struct ScoreKeeper final : Behaviour
    {
        Int32 Total = 0;

        void OnStart() override { Subscribe<&ScoreKeeper::OnScore>(); }

        void OnScore(const ScoreChanged& InEvent)
        {
            Note(*this, "Score");
            Total += InEvent.Score;
        }
    };

    /** Broadcasts, sends and creates from its OnDestroy. */
    struct Dier final : Behaviour
    {
        void OnDestroy() override
        {
            Broadcast(ScoreChanged{ 10 });
            Send(FindEntity(OpaaxString("Witness")), Hit{ 2 });
            CreateEntity(OpaaxString("Debris"));
            Note(*this, "Destroy");
        }
    };

    struct TimerUser final : Behaviour
    {
        Int32       OnceFired   = 0;
        Int32       RepeatFired = 0;
        Int32       LambdaFired = 0;
        TimerHandle Once;
        TimerHandle Repeat;
        TimerHandle Lambda;

        void OnStart() override
        {
            Once   = SetTimer<&TimerUser::OnOnce>(0.5f);
            Repeat = SetTimer<&TimerUser::OnRepeat>(0.25f, /*bInRepeat*/true);
            Lambda = SetTimer(1.f, [this]() { ++LambdaFired; });
        }

        void OnOnce()   { ++OnceFired; }
        void OnRepeat() { ++RepeatFired; }
    };

    struct Fuse final : Behaviour
    {
        void OnStart() override { DestroyAfter(0.25f); }
    };

    /** Handlers that change nothing of the behaviour's own state can be const members. */
    struct ConstHandlers final : Behaviour
    {
        mutable Int32 Timers = 0;
        mutable Int32 Scores = 0;

        void OnStart() override
        {
            SetTimer<&ConstHandlers::OnTimer>(0.25f);
            Subscribe<&ConstHandlers::OnScore>();
        }

        void OnTimer() const { ++Timers; }
        void OnScore(const ScoreChanged&) const { ++Scores; }
    };

    /** Spawns Prefab during its first update, then sends the new root a Hit. */
    struct Spawner final : Behaviour
    {
        OpaaxString Prefab;
        Entity      Spawned;
        bool        bDone = false;

        void OnUpdate(float) override
        {
            if (bDone) { return; }
            bDone = true;

            Spawned = Spawn(Prefab, Vector2F{ 10.f, 20.f }, 45.f);
            Note(*this, Spawned.IsValid() ? "Spawned" : "SpawnFailed");

            if (Spawned.IsValid()) { Send(Spawned, Hit{ 3 }); }
        }
    };

    /** Spawns Prefab from its OnStart and checks the spawned root has started. */
    struct StartSpawner final : Behaviour
    {
        OpaaxString Prefab;
        bool        bRootStarted = false;

        void OnStart() override
        {
            Entity lRoot = Spawn(Prefab, Vector2F{ 0.f, 0.f });
            bRootStarted = lRoot.IsValid() && lRoot.Get<HitCounter>().IsStarted();
        }
    };

    struct ContactProbe final : Behaviour
    {
        Int32    Collisions     = 0;
        Int32    Overlaps       = 0;
        EntityID LastOther      = ENTITY_NONE;
        bool     bLastWasSensor = false;

        void OnStart() override
        {
            Listen<&ContactProbe::OnCollision>();
            Listen<&ContactProbe::OnOverlap>();
        }

        void OnCollision(const CollisionBegan& InEvent)
        {
            ++Collisions;
            LastOther = InEvent.Other.GetHandle();
        }

        void OnOverlap(const OverlapBegan& InEvent)
        {
            ++Overlaps;
            LastOther      = InEvent.Other.GetHandle();
            bLastWasSensor = InEvent.bIsSensor;
        }
    };

    /** Launches its own body from OnStart, on the frame its entity was created. */
    struct Launcher final : Behaviour
    {
        void OnStart() override { SetVelocity(Vector2F{ 120.f, 0.f }); }
    };

    /** Kicks its entity every frame, body or not. */
    struct Kicker final : Behaviour
    {
        void OnUpdate(float) override { AddImpulse(Vector2F{ 1.f, 0.f }); }
    };

    /** Every frame: a ray 200 units down from its entity, and what is within 100 units of it. */
    struct Seeker final : Behaviour
    {
        Uint64            Channels = ~0ull;
        RayHit            Down;
        TDynArray<Entity> Around;

        void OnUpdate(float) override
        {
            const Vector2F lHere = GetWorldPosition();
            Down = RayCast(lHere, Vector2F{ 0.f, -1.f }, 200.f, Channels);
            OverlapBox(lHere - Vector2F{ 100.f, 100.f }, lHere + Vector2F{ 100.f, 100.f }, Around, Channels);
        }
    };

    /** Counts "Jump" starts twice: from a bound handler, and by polling in OnUpdate. */
    struct Jumper final : Behaviour
    {
        Int32 BoundJumps  = 0;
        Int32 PolledJumps = 0;
        bool  bHeld       = false;

        void OnStart() override { BindAction<&Jumper::OnJump>("Jump", EInputTrigger::Started); }
        void OnJump(const InputActionValue&) { ++BoundJumps; }

        void OnUpdate(float) override
        {
            if (WasActionStarted("Jump")) { ++PolledJumps; }
            bHeld = IsActionActive("Jump");
        }
    };

    struct KeyWatcher final : Behaviour
    {
        bool bDown     = false;
        bool bPressed  = false;
        bool bReleased = false;

        void OnUpdate(float) override
        {
            bDown     = IsKeyDown(EKeyCode::Space);
            bPressed  = WasKeyPressed(EKeyCode::Space);
            bReleased = WasKeyReleased(EKeyCode::Space);
        }
    };

    struct Quitter final : Behaviour
    {
        void OnUpdate(float) override { QuitGame(); }
    };

    struct Traveller final : Behaviour
    {
        void OnUpdate(float) override { OpenLevel(OpaaxString("Levels/Next.opaaxlevel")); }
    };

    /** Counts the started Probes it can see. */
    struct Finder final : Behaviour
    {
        Int32 Seen      = 0;
        bool  bFoundOne = false;

        void OnUpdate(float) override
        {
            Seen = 0;
            ForEachBehaviour<Probe>([this](Probe&) { ++Seen; });
            bFoundOne = (FindBehaviour<Probe>() != nullptr);
        }
    };

    struct Mover final : Behaviour
    {
        void OnUpdate(float) override { SetWorldPosition(Vector2F{ 150.f, 0.f }); }
    };

    /** Records the clock it sees. */
    struct Clock final : Behaviour
    {
        double Time  = 0.0;
        float  Delta = 0.f;

        void OnUpdate(float) override
        {
            Time  = GetTime();
            Delta = GetDeltaTime();
        }
    };
}

namespace
{
    namespace fs = std::filesystem;
    using namespace RuntimeProbes;

    void RegisterTypes(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<TransformComponent>(OpaaxStringID("Transform"), /*bEssential*/true));
        REQUIRE(InRegistry.Register<PrefabInstanceComponent>(OpaaxStringID("PrefabInstance")));
        REQUIRE(InRegistry.Register<Marker>(OpaaxStringID("Marker")));

        // Registration order is update order: Adder and Killer run before Probe.
        REQUIRE(InRegistry.Register<Adder>(OpaaxStringID("Adder")));
        REQUIRE(InRegistry.Register<Killer>(OpaaxStringID("Killer")));
        REQUIRE(InRegistry.Register<Probe>(OpaaxStringID("Probe")));
        REQUIRE(InRegistry.Register<Remover>(OpaaxStringID("Remover")));
        REQUIRE(InRegistry.Register<Ticker>(OpaaxStringID("Ticker")));
        REQUIRE(InRegistry.Register<HitCounter>(OpaaxStringID("HitCounter")));
        REQUIRE(InRegistry.Register<Armor>(OpaaxStringID("Armor")));
        REQUIRE(InRegistry.Register<Shooter>(OpaaxStringID("Shooter")));
        REQUIRE(InRegistry.Register<Greeter>(OpaaxStringID("Greeter")));
        REQUIRE(InRegistry.Register<ClickCounter>(OpaaxStringID("ClickCounter")));
        REQUIRE(InRegistry.Register<ClickWitness>(OpaaxStringID("ClickWitness")));
        REQUIRE(InRegistry.Register<Echo>(OpaaxStringID("Echo")));
        REQUIRE(InRegistry.Register<ScoreKeeper>(OpaaxStringID("ScoreKeeper")));
        REQUIRE(InRegistry.Register<Dier>(OpaaxStringID("Dier")));
        REQUIRE(InRegistry.Register<TimerUser>(OpaaxStringID("TimerUser")));
        REQUIRE(InRegistry.Register<ConstHandlers>(OpaaxStringID("ConstHandlers")));
        REQUIRE(InRegistry.Register<Fuse>(OpaaxStringID("Fuse")));
        REQUIRE(InRegistry.Register<Spawner>(OpaaxStringID("Spawner")));
        REQUIRE(InRegistry.Register<StartSpawner>(OpaaxStringID("StartSpawner")));
        REQUIRE(InRegistry.Register<ContactProbe>(OpaaxStringID("ContactProbe")));
        REQUIRE(InRegistry.Register<Launcher>(OpaaxStringID("Launcher")));
        REQUIRE(InRegistry.Register<Kicker>(OpaaxStringID("Kicker")));
        REQUIRE(InRegistry.Register<Seeker>(OpaaxStringID("Seeker")));
        REQUIRE(InRegistry.Register<Jumper>(OpaaxStringID("Jumper")));
        REQUIRE(InRegistry.Register<KeyWatcher>(OpaaxStringID("KeyWatcher")));
        REQUIRE(InRegistry.Register<Quitter>(OpaaxStringID("Quitter")));
        REQUIRE(InRegistry.Register<Traveller>(OpaaxStringID("Traveller")));
        REQUIRE(InRegistry.Register<Finder>(OpaaxStringID("Finder")));
        REQUIRE(InRegistry.Register<Mover>(OpaaxStringID("Mover")));
        REQUIRE(InRegistry.Register<Clock>(OpaaxStringID("Clock")));
    }

    /** The game's input mapping in the tests: "Jump" on Space. */
    void AddTestActions(InputMappingSubsystem& InMapping)
    {
        InputAction lJump;
        lJump.Name      = OpaaxStringID("Jump");
        lJump.ValueType = EInputValueType::Bool;
        REQUIRE(InMapping.RegisterAction(lJump));

        InputKeyBinding lSpace;
        lSpace.Action = OpaaxStringID("Jump");
        lSpace.Key    = EKeyCode::Space;

        InputMappingContext lContext;
        lContext.Name = OpaaxStringID("Test");
        lContext.Bindings.push_back(lSpace);
        REQUIRE(InMapping.AddContext(lContext));
    }

    /** What a case's world has besides behaviours. */
    struct FixtureOptions
    {
        const IPaths* Paths     = nullptr;   // null: IPaths::Null()
        bool          bPhysics  = false;
        bool          bActions  = false;     // the game's input mapping (AddTestActions)
        bool          bStartNow = true;
    };

    struct RuntimeFixture
    {
        ResourceManager   Resources;
        EngineEventBus    Events;
        DebugDraw         Debug;
        EngineConfigData  Config;
        InputManager      Input;
        ComponentRegistry Components;

        // The game's input mapping, when asked for. Declared before Worlds: as in the engine, the
        // game outlives its worlds.
        TUniquePtr<GameInstanceContext>   GameContext;
        TUniquePtr<InputMappingSubsystem> Actions;

        WorldManager Worlds;   // last: its worlds end before the services they use

        World*              TheWorld = nullptr;
        BehaviourSubsystem* Runtime  = nullptr;

        explicit RuntimeFixture(const FixtureOptions& InOptions = {})
        {
            Journal().clear();
            RegisterTypes(Components);

            const IPaths& lPaths = (InOptions.Paths != nullptr) ? *InOptions.Paths : IPaths::Null();

            if (InOptions.bActions)
            {
                GameContext = MakeUnique<GameInstanceContext>(GameInstanceContext{ Worlds, Resources, lPaths, Events,
                                                                                   Input, Config });
                Actions     = MakeUnique<InputMappingSubsystem>(*GameContext);
                AddTestActions(*Actions);
            }

            TheWorld = Worlds.CreateWorld("Behaviours", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, lPaths, Events, Input, Config,
                                               Actions.get(), /*UI*/ nullptr, Debug, &Components });

            WorldSubsystemMgr& lSubsystems = TheWorld->GetSubsystems();
            lSubsystems.RegisterSubsystem<BehaviourSubsystem>(std::ref(*TheWorld->GetContext()));
            if (InOptions.bPhysics)
            {
                lSubsystems.RegisterSubsystem<PhysicsSubsystem>(std::ref(*TheWorld->GetContext()));
            }

            if (InOptions.bStartNow)
            {
                Start();
            }
        }

        void Start()
        {
            TheWorld->GetSubsystems().StartupAll();
            Runtime = TheWorld->GetSubsystems().GetSubsystem<BehaviourSubsystem>();
            REQUIRE(Runtime != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));
        }

        Entity Make(const char* InName) const { return TheWorld->CreateEntity(OpaaxString(InName)); }

        Entity Find(const char* InName) const
        {
            EntityID lFound = ENTITY_NONE;
            TheWorld->Each<EntityMeta>([&](const EntityID InId, const EntityMeta& InMeta)
            {
                if (InMeta.Name == InName) { lFound = InId; }
            });
            return Entity(lFound, TheWorld);
        }

        /** One engine frame: the game's input mapping, the update, then one fixed step. */
        void Frame(const double InDeltaTime = 1.0 / 60.0)
        {
            if (Actions != nullptr) { Actions->Update(InDeltaTime); }
            Worlds.Update(InDeltaTime);
            Worlds.FixedUpdate(InDeltaTime);
        }

        void Frames(const Uint32 InCount, const double InDeltaTime = 1.0 / 60.0)
        {
            for (Uint32 lFrame = 0; lFrame < InCount; ++lFrame) { Frame(InDeltaTime); }
        }
    };

    // Every asset under one temp root, so AssetToAbsolute is a concatenation.
    class TempPaths final : public IPaths
    {
    public:
        explicit TempPaths(const fs::path& InRoot) : m_Root(InRoot) {}

        OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel) const override
        {
            return OpaaxString((m_Root / InAssetRel.CStr()).generic_string().c_str());
        }

        OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath) const override
        {
            return OpaaxString(fs::path(InAbsPath.CStr()).lexically_relative(m_Root).generic_string().c_str());
        }

        void        LogPaths()      const override {}
        OpaaxString WorkspaceRoot() const override { return Root(); }
        OpaaxString EngineRoot()    const override { return Root(); }
        OpaaxString ProjectRoot()   const override { return Root(); }
        OpaaxString ProjectFile()   const override { return Root(); }
        OpaaxString AssetsDir()     const override { return Root(); }
        OpaaxString ConfigsDir()    const override { return Root(); }
        OpaaxString SourceDir()     const override { return Root(); }
        OpaaxString SaveDir()       const override { return Root(); }
        OpaaxString TempDir()       const override { return Root(); }

        OpaaxString EngineToAbsolute(const OpaaxString& InRel)  const override { return AssetToAbsolute(InRel); }
        OpaaxString ProjectToAbsolute(const OpaaxString& InRel) const override { return AssetToAbsolute(InRel); }

    private:
        OpaaxString Root() const { return OpaaxString(m_Root.generic_string().c_str()); }

        fs::path m_Root;
    };

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
            : m_Path(fs::temp_directory_path() / ("OpaaxBehaviourRuntimeTests_" + std::string(InTag)))
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
            fs::create_directories(m_Path / "Prefabs", lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        const fs::path& Root() const { return m_Path; }

    private:
        fs::path m_Path;
    };

    /** Prefabs/Bullet.opaaxprefab: a root "Bullet" with a HitCounter, and a child "Trail" with a Probe. */
    void WriteBulletPrefab(const IPaths& InPaths)
    {
        EntityData lRoot;
        lRoot.Id   = Guid::New();
        lRoot.Name = OpaaxString("Bullet");
        lRoot.Components.emplace_back(OpaaxStringID("HitCounter"), nlohmann::json::object());

        EntityData lTrail;
        lTrail.Id     = Guid::New();
        lTrail.Name   = OpaaxString("Trail");
        lTrail.Parent = lRoot.Id;
        lTrail.Components.emplace_back(OpaaxStringID("Probe"), nlohmann::json::object());

        PrefabData lPrefab;
        lPrefab.Entities.emplace_back(Move(lRoot));
        lPrefab.Entities.emplace_back(Move(lTrail));

        REQUIRE(PrefabFile::Save(InPaths.AssetToAbsolute(OpaaxString("Prefabs/Bullet.opaaxprefab")), lPrefab));
    }

    Entity MakeCollider(RuntimeFixture& InFix, const char* InName, const Vector2F& InPosition, const Vector2F& InSize,
                        const EColliderMode InMode, const bool bInDynamic)
    {
        Entity lEntity = InFix.Make(InName);
        lEntity.Get<TransformComponent>().Position = InPosition;

        ColliderComponent lCollider;
        lCollider.Shape = EColliderShape::Box;
        lCollider.Size  = InSize;
        lCollider.Mode  = InMode;
        lEntity.AddOrReplace<ColliderComponent>(lCollider);

        if (bInDynamic)
        {
            RigidbodyComponent lBody;
            lBody.Type = EBodyType::Dynamic;
            lEntity.AddOrReplace<RigidbodyComponent>(lBody);
        }

        lEntity.Add<ContactProbe>();
        return lEntity;
    }
}

// =============================================================================
// Lifecycle
// =============================================================================
TEST_CASE("Behaviours: OnStart runs once, before the first update and the first fixed step")
{
    RuntimeFixture lFix;
    lFix.Make("A").Add<Probe>();

    // Nothing runs before the world ticks.
    CHECK(Journal().empty());
    CHECK(lFix.Runtime->GetPendingCount() == 1);

    lFix.Frame();
    CHECK(Journal() == std::vector<std::string>{ "A:Start", "A:Update", "A:Fixed" });

    lFix.Frame();
    CHECK(CountOf("A:Start") == 1);
    CHECK(CountOf("A:Update") == 2);
    CHECK(CountOf("A:Fixed") == 2);
    CHECK(lFix.Runtime->GetStartedCount() == 1);
    CHECK(lFix.Runtime->GetPendingCount() == 0);
}

TEST_CASE("Behaviours: a behaviour already in the world when it starts is started like a new one")
{
    RuntimeFixture lFix({ .bStartNow = false });
    lFix.Make("Early").Add<Probe>();

    lFix.Start();
    CHECK(Journal().empty());

    lFix.Frame();
    CHECK(Journal() == std::vector<std::string>{ "Early:Start", "Early:Update", "Early:Fixed" });
}

TEST_CASE("Behaviours: one added during a pass starts in that pass and is first updated in the next")
{
    RuntimeFixture lFix;
    lFix.Make("Maker").Add<Adder>();

    lFix.Worlds.Update(1.0 / 60.0);

    // Started after the Adder type's turn; the Probe type's turn came later in the same pass and
    // still skipped it.
    CHECK(CountOf("Late:Start") == 1);
    CHECK(CountOf("Late:Update") == 0);

    lFix.Worlds.FixedUpdate(1.0 / 60.0);
    CHECK(CountOf("Late:Fixed") == 1);

    lFix.Frame();
    CHECK(CountOf("Late:Update") == 1);
    CHECK(CountOf("Late:Start") == 1);
}

TEST_CASE("Behaviours: copying a behaviour copies its fields, never its runtime binding")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Probe>().DestroySelfOnUpdate = false;
    lFix.Frame();

    const Probe& lLive = lEntity.Get<Probe>();
    REQUIRE(lLive.IsStarted());

    const Probe lCopy = lLive;
    CHECK_FALSE(lCopy.IsStarted());
    CHECK_FALSE(lCopy.GetEntity().IsValid());
}

TEST_CASE("Behaviours: calls before OnStart are ignored")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    Probe& lProbe  = lEntity.Add<Probe>();

    // Not started: no runtime to ask.
    lProbe.Destroy();
    CHECK_FALSE(lProbe.SetTimer(1.f, []() {}).IsValid());

    lFix.Frames(2);
    CHECK(lEntity.IsValid());
    CHECK(CountOf("A:Destroy") == 0);
}

TEST_CASE("Behaviours: Edit worlds run no behaviours")
{
    const World lEdit(OpaaxString("Edit"), EWorldMode::Edit);
    const World lPlay(OpaaxString("Play"), EWorldMode::Play);

    CHECK_FALSE(BehaviourSubsystem::ShouldCreate(lEdit));
    CHECK(BehaviourSubsystem::ShouldCreate(lPlay));
}

// =============================================================================
// Destruction
// =============================================================================
TEST_CASE("Behaviours: Destroy waits for the end of the pass; the target's later update is skipped")
{
    RuntimeFixture lFix;
    Entity lVictim = lFix.Make("Victim");
    lVictim.Add<Probe>();
    lFix.Make("Hunter").Add<Killer>().Target = OpaaxString("Victim");

    lFix.Worlds.Update(1.0 / 60.0);

    // Started with the batch; Killer ran first (registration order) and its target's update was skipped.
    CHECK(CountOf("Victim:Start") == 1);
    CHECK(CountOf("Victim:Update") == 0);
    CHECK(CountOf("Victim:Destroy") == 1);
    CHECK_FALSE(lVictim.IsValid());
    CHECK(lFix.Runtime->GetStartedCount() == 1);   // the Killer
}

TEST_CASE("Behaviours: a behaviour destroying its own entity finishes its update first")
{
    RuntimeFixture lFix;
    Entity lSelf = lFix.Make("Self");
    lSelf.Add<Probe>().DestroySelfOnUpdate = true;

    lFix.Worlds.Update(1.0 / 60.0);

    CHECK(Journal() == std::vector<std::string>{ "Self:Start", "Self:Update", "Self:Destroy" });
    CHECK_FALSE(lSelf.IsValid());
}

TEST_CASE("Behaviours: OnDestroy runs once, with the entity whole, whatever destroys it")
{
    SUBCASE("Destroy from gameplay code")
    {
        RuntimeFixture lFix;
        Entity lEntity = lFix.Make("A");
        lEntity.Add<Marker>();
        lEntity.Add<Probe>().DestroySelfOnUpdate = true;

        lFix.Frames(2);
        CHECK(CountOf("A:Destroy") == 1);
        CHECK(CountOf("A:Whole") == 1);
        CHECK(CountOf("A:WorldEnding") == 0);
    }

    SUBCASE("the world destroying the entity outright")
    {
        RuntimeFixture lFix;
        Entity lEntity = lFix.Make("A");
        lEntity.Add<Marker>();
        lEntity.Add<Probe>();
        lFix.Frame();

        lFix.TheWorld->DestroyEntity(lEntity);
        CHECK(CountOf("A:Destroy") == 1);
        CHECK(CountOf("A:Whole") == 1);

        lFix.Frames(2);
        CHECK(CountOf("A:Destroy") == 1);
    }

    SUBCASE("the parent's destruction taking its children")
    {
        RuntimeFixture lFix;
        Entity lParent = lFix.Make("Parent");
        Entity lChild  = lFix.Make("Child");
        REQUIRE(EntityHierarchy::SetParent(lChild, lParent));
        lParent.Add<Marker>();
        lChild.Add<Marker>();
        lParent.Add<Probe>();
        lChild.Add<Probe>();
        lFix.Frame();

        lFix.TheWorld->DestroyEntity(lParent);
        CHECK(CountOf("Parent:Whole") == 1);
        CHECK(CountOf("Child:Whole") == 1);

        // The parent is told first, while its children are still there.
        CHECK(IndexOf("Parent:Destroy") < IndexOf("Child:Destroy"));
        CHECK_FALSE(lChild.IsValid());
    }

    SUBCASE("the world clearing every entity")
    {
        RuntimeFixture lFix;
        Entity lEntity = lFix.Make("A");
        lEntity.Add<Marker>();
        lEntity.Add<Probe>();
        lFix.Frame();

        lFix.TheWorld->Clear();
        CHECK(CountOf("A:Destroy") == 1);
        CHECK(CountOf("A:Whole") == 1);
    }

    SUBCASE("the world ending")
    {
        {
            RuntimeFixture lFix;
            Entity lEntity = lFix.Make("A");
            lEntity.Add<Marker>();
            lEntity.Add<Probe>();
            lFix.Frame();
            CHECK(CountOf("A:Destroy") == 0);
        }

        CHECK(CountOf("A:Destroy") == 1);
        CHECK(CountOf("A:Whole") == 1);
        CHECK(CountOf("A:WorldEnding") == 1);
    }
}

TEST_CASE("Behaviours: Remove ends one behaviour at the end of the pass; the entity and the others stay")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Marker>();
    lEntity.Add<Probe>();
    lEntity.Add<Ticker>();
    lEntity.Add<Remover>();

    lFix.Frame();
    CHECK(CountOf("A:Destroy") == 1);
    CHECK(CountOf("A:Whole") == 1);
    CHECK_FALSE(lEntity.Has<Probe>());
    CHECK(lEntity.IsValid());

    lFix.Frames(2);
    CHECK(CountOf("A:Destroy") == 1);
    CHECK(lEntity.Get<Ticker>().Updates == 3);
}

TEST_CASE("Behaviours: one removed before it started gets neither OnStart nor OnDestroy")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Probe>();
    lEntity.Remove<Probe>();

    lFix.Frames(2);
    CHECK(Journal().empty());
    CHECK(lFix.Runtime->GetStartedCount() == 0);
}

TEST_CASE("Behaviours: OnDestroy can still broadcast, send and create entities")
{
    RuntimeFixture lFix;
    lFix.Make("Keeper").Add<ScoreKeeper>();
    Entity lWitness = lFix.Make("Witness");
    lWitness.Add<HitCounter>();
    Entity lDier = lFix.Make("Dier");
    lDier.Add<Dier>();
    lFix.Frame();

    lFix.TheWorld->DestroyEntity(lDier);

    CHECK(lFix.Find("Keeper").Get<ScoreKeeper>().Total == 10);
    CHECK(lWitness.Get<HitCounter>().Total == 2);
    CHECK(lFix.Find("Debris").IsValid());
}

// =============================================================================
// Entity events
// =============================================================================
TEST_CASE("Behaviours: Send reaches the target's listeners only, each handler once")
{
    RuntimeFixture lFix;
    Entity lTarget = lFix.Make("Target");
    lTarget.Add<HitCounter>();
    lTarget.Add<Armor>();
    Entity lBystander = lFix.Make("Bystander");
    lBystander.Add<HitCounter>();
    lFix.Make("Gunner").Add<Shooter>().Target = OpaaxString("Target");

    lFix.Frame();

    CHECK(lTarget.Get<HitCounter>().Hits == 1);   // listened twice, called once
    CHECK(lTarget.Get<HitCounter>().Total == 5);
    CHECK(lTarget.Get<Armor>().Hits == 1);
    CHECK(lBystander.Get<HitCounter>().Hits == 0);
}

TEST_CASE("Behaviours: a bubbling event climbs the parents until a handler stops it")
{
    RuntimeFixture lFix;
    Entity lGrandparent = lFix.Make("Grandparent");
    Entity lParent      = lFix.Make("Parent");
    Entity lChild       = lFix.Make("Child");
    REQUIRE(EntityHierarchy::SetParent(lParent, lGrandparent));
    REQUIRE(EntityHierarchy::SetParent(lChild, lParent));

    lGrandparent.Add<ClickCounter>();
    lParent.Add<ClickCounter>();
    lParent.Add<ClickWitness>();
    lChild.Add<ClickCounter>();
    lFix.Frame();

    lFix.Runtime->Send(lChild.GetHandle(), Clicked{ 1 });
    CHECK(lChild.Get<ClickCounter>().Clicks == 1);
    CHECK(lParent.Get<ClickCounter>().Clicks == 1);
    CHECK(lGrandparent.Get<ClickCounter>().Clicks == 1);

    SUBCASE("StopPropagation: no further up, but the same entity's other listeners still hear it")
    {
        lParent.Get<ClickCounter>().bStop = true;
        lFix.Runtime->Send(lChild.GetHandle(), Clicked{ 2 });

        CHECK(lChild.Get<ClickCounter>().Clicks == 2);
        CHECK(lParent.Get<ClickCounter>().Clicks == 2);
        CHECK(lParent.Get<ClickWitness>().Clicks == 2);
        CHECK(lGrandparent.Get<ClickCounter>().Clicks == 1);
    }

    SUBCASE("an event that does not bubble stays on its target")
    {
        lChild.Add<HitCounter>();
        lParent.Add<HitCounter>();
        lFix.Frame();

        lFix.Runtime->Send(lChild.GetHandle(), Hit{ 1 });
        CHECK(lChild.Get<HitCounter>().Hits == 1);
        CHECK(lParent.Get<HitCounter>().Hits == 0);
    }
}

TEST_CASE("Behaviours: handlers sending to each other in a loop stop at the nesting limit")
{
    RuntimeFixture lFix;
    Entity lA = lFix.Make("A");
    Entity lB = lFix.Make("B");
    lA.Add<Echo>().Partner = OpaaxString("B");
    lB.Add<Echo>().Partner = OpaaxString("A");
    lFix.Frame();

    lFix.Runtime->Send(lA.GetHandle(), Ping{});

    // 32 nested sends are delivered, the 33rd is dropped (logged once).
    CHECK(lA.Get<Echo>().Pings + lB.Get<Echo>().Pings == 32);

    // The guard resets: the next send goes through again.
    lFix.Runtime->Send(lA.GetHandle(), Ping{});
    CHECK(lA.Get<Echo>().Pings + lB.Get<Echo>().Pings == 64);
}

TEST_CASE("Behaviours: an event sent from OnStart reaches a behaviour of the same batch not started yet")
{
    RuntimeFixture lFix;
    lFix.Make("A").Add<Greeter>().Target = OpaaxString("B");
    Entity lB = lFix.Make("B");
    lB.Add<HitCounter>();

    lFix.Frame();

    // B was started on demand, so it was listening when A's event arrived; it started only once.
    CHECK(lB.Get<HitCounter>().Hits == 1);
    CHECK(CountOf("B:Start") == 1);
    CHECK(IndexOf("B:Start") < IndexOf("B:Hit"));
    CHECK(IndexOf("A:Start") < IndexOf("B:Start"));
}

TEST_CASE("Behaviours: events from physics reach the behaviours on both entities")
{
    SUBCASE("two solid colliders")
    {
        RuntimeFixture lFix({ .bPhysics = true });
        Entity lGround = MakeCollider(lFix, "Ground", { 0.f, 0.f }, { 800.f, 100.f }, EColliderMode::Solid, false);
        Entity lBall   = MakeCollider(lFix, "Ball", { 0.f, 300.f }, { 50.f, 50.f }, EColliderMode::Solid, true);

        lFix.Frames(120);

        CHECK(lGround.Get<ContactProbe>().Collisions >= 1);
        CHECK(lBall.Get<ContactProbe>().Collisions >= 1);
        CHECK(lGround.Get<ContactProbe>().LastOther == lBall.GetHandle());
        CHECK(lBall.Get<ContactProbe>().LastOther == lGround.GetHandle());
    }

    SUBCASE("a body passing through a sensor")
    {
        RuntimeFixture lFix({ .bPhysics = true });
        Entity lSensor = MakeCollider(lFix, "Sensor", { 0.f, 0.f }, { 400.f, 40.f }, EColliderMode::Overlap, false);
        Entity lFaller = MakeCollider(lFix, "Faller", { 0.f, 400.f }, { 40.f, 40.f }, EColliderMode::Solid, true);

        lFix.Frames(180);

        const ContactProbe& lSensorSide = lSensor.Get<ContactProbe>();
        const ContactProbe& lFallerSide = lFaller.Get<ContactProbe>();
        CHECK(lSensorSide.Overlaps == 1);
        CHECK(lFallerSide.Overlaps == 1);
        CHECK(lSensorSide.bLastWasSensor);
        CHECK_FALSE(lFallerSide.bLastWasSensor);
        CHECK(lSensorSide.LastOther == lFaller.GetHandle());
        CHECK(lFallerSide.LastOther == lSensor.GetHandle());
    }
}

TEST_CASE("Behaviours: physics events from another world are ignored")
{
    RuntimeFixture lFix;
    Entity lA = lFix.Make("A");
    Entity lB = lFix.Make("B");
    lA.Add<ContactProbe>();
    lB.Add<ContactProbe>();
    lFix.Frame();

    const World lOther(OpaaxString("Other"));
    lFix.Events.GetEventBus().Publish(PhysicsCollisionBegan{ lA.GetHandle(), lB.GetHandle(), &lOther });
    CHECK(lA.Get<ContactProbe>().Collisions == 0);

    lFix.Events.GetEventBus().Publish(PhysicsCollisionBegan{ lA.GetHandle(), lB.GetHandle(), lFix.TheWorld });
    CHECK(lA.Get<ContactProbe>().Collisions == 1);
    CHECK(lB.Get<ContactProbe>().Collisions == 1);
}

// =============================================================================
// Global events
// =============================================================================
TEST_CASE("Behaviours: a const member can handle a timer and an event")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("Const");
    lEntity.Add<ConstHandlers>();
    lFix.Frame();

    lFix.Worlds.Update(0.5);
    lFix.Events.GetEventBus().Publish(ScoreChanged{ 3 });

    CHECK(lEntity.Get<ConstHandlers>().Timers == 1);
    CHECK(lEntity.Get<ConstHandlers>().Scores == 1);
}

TEST_CASE("Behaviours: Subscribe receives engine events until the behaviour ends")
{
    RuntimeFixture lFix;
    Entity lKeeper = lFix.Make("Keeper");
    lKeeper.Add<ScoreKeeper>();
    lFix.Frame();

    EventBus& lBus = lFix.Events.GetEventBus();
    lBus.Publish(ScoreChanged{ 5 });
    lBus.Publish(ScoreChanged{ 2 });
    CHECK(lKeeper.Get<ScoreKeeper>().Total == 7);

    lFix.TheWorld->DestroyEntity(lKeeper);
    lBus.Publish(ScoreChanged{ 1 });
    CHECK(CountOf("Keeper:Score") == 2);
}

TEST_CASE("Behaviours: QuitGame asks the application to quit, at the start of the next frame")
{
    RuntimeFixture lFix;
    lFix.Make("A").Add<Quitter>();

    bool bQuit = false;
    const DelegateHandle lHandle = lFix.Events.GetEventBus().Subscribe<QuitGameRequested>(
        [&bQuit](const QuitGameRequested&) { bQuit = true; });

    // Never inside the world's update: the host may destroy the world when it quits.
    lFix.Frame();
    CHECK_FALSE(bQuit);

    lFix.Events.GetEventBus().Flush();
    CHECK(bQuit);

    lFix.Events.GetEventBus().Unsubscribe(lHandle);
}

TEST_CASE("Behaviours: OpenLevel without a running engine is a harmless request")
{
    // No application here: the engine service is its null object, which ignores the request.
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Traveller>();

    lFix.Frames(2);
    CHECK(lEntity.IsValid());
    CHECK(lFix.Runtime->GetStartedCount() == 1);
}

// =============================================================================
// Timers
// =============================================================================
TEST_CASE("Behaviours: timers count world time from when they are set")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<TimerUser>();

    // Exact steps in binary, so the due times compare exactly.
    const auto lStep = [&lFix]() { lFix.Worlds.Update(0.125); };
    const TimerUser& lUser = lEntity.Get<TimerUser>();

    for (Int32 lFrame = 0; lFrame < 4; ++lFrame) { lStep(); }   // t = 0.5: set at 0.125
    CHECK(lUser.OnceFired == 0);
    CHECK(lUser.RepeatFired == 1);

    lStep();                                                     // t = 0.625
    CHECK(lUser.OnceFired == 1);
    CHECK(lUser.RepeatFired == 2);

    SUBCASE("a repeating timer stops when cleared; a one-shot fires once")
    {
        lEntity.Get<TimerUser>().ClearTimer(lUser.Repeat);
        for (Int32 lFrame = 0; lFrame < 4; ++lFrame) { lStep(); }   // t = 1.125

        CHECK(lUser.RepeatFired == 2);
        CHECK(lUser.OnceFired == 1);
        CHECK(lUser.LambdaFired == 1);
        CHECK(lFix.Runtime->GetTimerCount() == 0);
    }

    SUBCASE("a long frame fires a repeating timer once, not once per missed interval")
    {
        lFix.Worlds.Update(1.0);
        CHECK(lUser.RepeatFired == 3);

        lStep();
        CHECK(lUser.RepeatFired == 4);
    }

    SUBCASE("the behaviour's end clears its timers")
    {
        CHECK(lFix.Runtime->GetTimerCount() == 2);   // the repeat and the lambda
        lFix.TheWorld->DestroyEntity(lEntity);
        CHECK(lFix.Runtime->GetTimerCount() == 0);

        lStep();
    }
}

TEST_CASE("Behaviours: DestroyAfter destroys the entity once the delay has passed")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Fuse>();

    lFix.Worlds.Update(0.125);   // starts; due at 0.375
    lFix.Worlds.Update(0.125);
    CHECK(lEntity.IsValid());

    lFix.Worlds.Update(0.125);
    CHECK_FALSE(lEntity.IsValid());
}

TEST_CASE("Behaviours: the clock is the sum of the update steps")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Clock>();

    lFix.Worlds.Update(0.25);
    lFix.Worlds.Update(0.5);

    CHECK(lEntity.Get<Clock>().Time == doctest::Approx(0.75));
    CHECK(lEntity.Get<Clock>().Delta == doctest::Approx(0.5f));
    CHECK(lFix.Runtime->GetTime() == doctest::Approx(0.75));
}

// =============================================================================
// Spawning
// =============================================================================
TEST_CASE("Behaviours: Spawn places a prefab and starts it before returning")
{
    const ScopedTempDir lDir("spawn");
    const TempPaths     lPaths(lDir.Root());
    WriteBulletPrefab(lPaths);

    RuntimeFixture lFix({ .Paths = &lPaths });
    Entity lGun = lFix.Make("Gun");
    lGun.Add<Spawner>().Prefab = OpaaxString("Prefabs/Bullet.opaaxprefab");

    lFix.Frame();

    const Entity lBullet = lGun.Get<Spawner>().Spawned;
    REQUIRE(lBullet.IsValid());

    Entity lRoot = lBullet;
    CHECK(lRoot.Get<EntityMeta>().Name == "Bullet");
    CHECK(lRoot.Get<TransformComponent>().Position.x == doctest::Approx(10.f));
    CHECK(lRoot.Get<TransformComponent>().Position.y == doctest::Approx(20.f));
    CHECK(lRoot.Get<TransformComponent>().Rotation == doctest::Approx(45.f));

    // Started inside Spawn: the Hit sent on the next line found it listening.
    CHECK(lRoot.Get<HitCounter>().Hits == 1);
    CHECK(IndexOf("Bullet:Start") < IndexOf("Gun:Spawned"));

    // The child came with it, under the root. Nothing spawned belongs to a map (never saved).
    const Entity lTrail = lFix.Find("Trail");
    REQUIRE(lTrail.IsValid());
    CHECK(EntityHierarchy::GetParent(lTrail).GetHandle() == lRoot.GetHandle());
    CHECK(CountOf("Trail:Start") == 1);
    CHECK_FALSE(lRoot.Get<EntityMeta>().OwnerMap.IsValid());
}

TEST_CASE("Behaviours: Spawn from an OnStart also starts what it spawns before returning")
{
    const ScopedTempDir lDir("spawn_onstart");
    const TempPaths     lPaths(lDir.Root());
    WriteBulletPrefab(lPaths);

    RuntimeFixture lFix({ .Paths = &lPaths });
    Entity lMaker = lFix.Make("Maker");
    lMaker.Add<StartSpawner>().Prefab = OpaaxString("Prefabs/Bullet.opaaxprefab");

    lFix.Frame();

    CHECK(lMaker.Get<StartSpawner>().bRootStarted);
    CHECK(CountOf("Bullet:Start") == 1);
    CHECK(CountOf("Trail:Start") == 1);
}

TEST_CASE("Behaviours: Spawn of a missing prefab gives an invalid entity")
{
    const ScopedTempDir lDir("missing");
    const TempPaths     lPaths(lDir.Root());

    RuntimeFixture lFix({ .Paths = &lPaths });
    Entity lGun = lFix.Make("Gun");
    lGun.Add<Spawner>().Prefab = OpaaxString("Prefabs/Nothing.opaaxprefab");

    lFix.Frame();

    CHECK_FALSE(lGun.Get<Spawner>().Spawned.IsValid());
    CHECK(CountOf("Gun:SpawnFailed") == 1);
}

// =============================================================================
// Physics
// =============================================================================
TEST_CASE("Behaviours: a behaviour drives its own body, from the frame its entity is created")
{
    RuntimeFixture lFix({ .bPhysics = true });
    lFix.TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>()->GetPhysicsWorld()->SetGravity({ 0.f, 0.f });

    Entity lBall = MakeCollider(lFix, "Ball", { 0.f, 0.f }, { 50.f, 50.f }, EColliderMode::Solid, /*dynamic*/true);
    lBall.Add<Launcher>();

    lFix.Frames(60);   // one second

    CHECK(lBall.Get<TransformComponent>().Position.x == doctest::Approx(120.f).epsilon(0.02));
    CHECK(lBall.Get<Launcher>().GetVelocity().x == doctest::Approx(120.f));
    CHECK(lBall.Get<Launcher>().GetMass() > 0.f);
}

TEST_CASE("Behaviours: physics calls on an entity without a body do nothing")
{
    SUBCASE("a world with physics, an entity without a collider")
    {
        RuntimeFixture lFix({ .bPhysics = true });
        Entity lGhost = lFix.Make("Ghost");
        lGhost.Add<Kicker>();

        lFix.Frames(3);
        CHECK(lGhost.IsValid());
        CHECK(lGhost.Get<Kicker>().GetVelocity().x == 0.f);
        CHECK(lGhost.Get<Kicker>().GetMass() == 0.f);
    }

    SUBCASE("a world without physics")
    {
        RuntimeFixture lFix;
        Entity lGhost = lFix.Make("Ghost");
        lGhost.Add<Kicker>();

        lFix.Frames(3);
        CHECK(lGhost.IsValid());
        CHECK(lGhost.Get<Kicker>().GetVelocity().x == 0.f);
    }
}

TEST_CASE("Behaviours: RayCast finds the closest collider, not the one it starts in")
{
    RuntimeFixture lFix({ .bPhysics = true });
    lFix.TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>()->GetPhysicsWorld()->SetGravity({ 0.f, 0.f });

    Entity lSeeker = MakeCollider(lFix, "Seeker", { 0.f, 0.f }, { 40.f, 40.f }, EColliderMode::Solid, /*dynamic*/true);
    Entity lGround = MakeCollider(lFix, "Ground", { 0.f, -100.f }, { 400.f, 20.f }, EColliderMode::Solid, /*dynamic*/false);
    MakeCollider(lFix, "Deeper", { 0.f, -150.f }, { 400.f, 20.f }, EColliderMode::Solid, /*dynamic*/false);
    lSeeker.Add<Seeker>();

    lFix.Frames(2);

    const RayHit& lHit = lSeeker.Get<Seeker>().Down;
    REQUIRE(static_cast<bool>(lHit));
    CHECK(lHit.Target.GetHandle() == lGround.GetHandle());
    CHECK(lHit.Point.y == doctest::Approx(-90.f).epsilon(0.001));
    CHECK(lHit.Normal.y == doctest::Approx(1.f));
    CHECK(lHit.Fraction == doctest::Approx(90.f / 200.f).epsilon(0.001));
}

TEST_CASE("Behaviours: OverlapBox lists the entities whose colliders are in the box")
{
    RuntimeFixture lFix({ .bPhysics = true });
    lFix.TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>()->GetPhysicsWorld()->SetGravity({ 0.f, 0.f });

    Entity lSeeker = MakeCollider(lFix, "Seeker", { 0.f, 0.f }, { 40.f, 40.f }, EColliderMode::Solid, /*dynamic*/true);
    Entity lNear   = MakeCollider(lFix, "Near", { 60.f, 0.f }, { 20.f, 20.f }, EColliderMode::Overlap, /*dynamic*/false);
    MakeCollider(lFix, "Far", { 500.f, 0.f }, { 20.f, 20.f }, EColliderMode::Solid, /*dynamic*/false);
    lSeeker.Add<Seeker>();

    lFix.Frames(2);

    const TDynArray<Entity>& lAround = lSeeker.Get<Seeker>().Around;
    const auto lHas = [&lAround](const Entity InEntity)
    {
        return std::any_of(lAround.begin(), lAround.end(),
                           [InEntity](const Entity InFound) { return InFound.GetHandle() == InEntity.GetHandle(); });
    };

    CHECK(lAround.size() == 2);
    CHECK(lHas(lSeeker));
    CHECK(lHas(lNear));
}

TEST_CASE("Behaviours: physics queries filter by channel, and find nothing without physics")
{
    SUBCASE("a channel no collider is on")
    {
        RuntimeFixture lFix({ .bPhysics = true });
        lFix.TheWorld->GetSubsystems().GetSubsystem<PhysicsSubsystem>()->GetPhysicsWorld()->SetGravity({ 0.f, 0.f });

        Entity lSeeker = MakeCollider(lFix, "Seeker", { 0.f, 0.f }, { 40.f, 40.f }, EColliderMode::Solid, /*dynamic*/true);
        MakeCollider(lFix, "Ground", { 0.f, -100.f }, { 400.f, 20.f }, EColliderMode::Solid, /*dynamic*/false);
        lSeeker.Add<Seeker>().Channels = CategoryBit(ECollisionChannel::Projectile);

        lFix.Frames(2);

        CHECK_FALSE(static_cast<bool>(lSeeker.Get<Seeker>().Down));
        CHECK(lSeeker.Get<Seeker>().Around.empty());
    }

    SUBCASE("a world without physics")
    {
        RuntimeFixture lFix;
        Entity lSeeker = lFix.Make("Seeker");
        lSeeker.Add<Seeker>();

        lFix.Frames(2);

        CHECK_FALSE(static_cast<bool>(lSeeker.Get<Seeker>().Down));
        CHECK(lSeeker.Get<Seeker>().Around.empty());
    }
}

TEST_CASE("Behaviours: GetSubsystem finds the world's subsystems")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<Probe>();
    lFix.Frame();

    const Probe& lProbe = lEntity.Get<Probe>();
    CHECK(lProbe.GetSubsystem<BehaviourSubsystem>() == lFix.Runtime);
    CHECK(lProbe.GetSubsystem<PhysicsSubsystem>() == nullptr);
}

// =============================================================================
// Queries, transform, input
// =============================================================================
TEST_CASE("Behaviours: FindBehaviour and ForEachBehaviour see started behaviours only")
{
    RuntimeFixture lFix;
    lFix.Make("Finder").Add<Finder>();
    lFix.Frame();
    CHECK(lFix.Find("Finder").Get<Finder>().Seen == 0);
    CHECK_FALSE(lFix.Find("Finder").Get<Finder>().bFoundOne);

    lFix.Make("A").Add<Probe>();
    lFix.Make("B").Add<Probe>();
    lFix.Frame();
    CHECK(lFix.Find("Finder").Get<Finder>().Seen == 2);
    CHECK(lFix.Find("Finder").Get<Finder>().bFoundOne);
}

TEST_CASE("Behaviours: SetWorldPosition goes through the parent")
{
    RuntimeFixture lFix;
    Entity lParent = lFix.Make("Parent");
    Entity lChild  = lFix.Make("Child");
    lParent.Get<TransformComponent>().Position = Vector2F{ 100.f, 0.f };
    REQUIRE(EntityHierarchy::SetParent(lChild, lParent, /*bInKeepWorld*/false));
    lChild.Add<Mover>();

    lFix.Frame();

    CHECK(lChild.Get<TransformComponent>().Position.x == doctest::Approx(50.f));
    CHECK(EntityHierarchy::WorldTransform(lChild).Position.x == doctest::Approx(150.f));
}

TEST_CASE("Behaviours: input actions are polled and bound; the binding ends with the behaviour")
{
    RuntimeFixture lFix({ .bActions = true });
    Entity lPlayer = lFix.Make("Player");
    lPlayer.Add<Jumper>();

    lFix.Frame();   // starts, binds
    CHECK(lFix.Actions->GetBindingCount() == 1);

    lFix.Input.OnKeyPressed(EKeyCode::Space, /*InRepeat*/false);
    lFix.Frame();
    CHECK(lPlayer.Get<Jumper>().BoundJumps == 1);
    CHECK(lPlayer.Get<Jumper>().PolledJumps == 1);
    CHECK(lPlayer.Get<Jumper>().bHeld);

    // Still held: active, but no new start.
    lFix.Input.EndFrame();
    lFix.Frame();
    CHECK(lPlayer.Get<Jumper>().BoundJumps == 1);
    CHECK(lPlayer.Get<Jumper>().PolledJumps == 1);
    CHECK(lPlayer.Get<Jumper>().bHeld);

    lFix.TheWorld->DestroyEntity(lPlayer);
    CHECK(lFix.Actions->GetBindingCount() == 0);

    SUBCASE("a world without the game's input mapping reads every action as zero")
    {
        RuntimeFixture lBare;
        Entity lAlone = lBare.Make("Alone");
        lAlone.Add<Jumper>();
        lBare.Input.OnKeyPressed(EKeyCode::Space, false);
        lBare.Frame();

        CHECK(lAlone.Get<Jumper>().PolledJumps == 0);
        CHECK_FALSE(lAlone.Get<Jumper>().bHeld);
        CHECK(lAlone.Get<Jumper>().GetAction("Jump").Value.x == 0.f);
    }
}

TEST_CASE("Behaviours: input is read from the frame's state")
{
    RuntimeFixture lFix;
    Entity lEntity = lFix.Make("A");
    lEntity.Add<KeyWatcher>();
    const KeyWatcher& lWatcher = lEntity.Get<KeyWatcher>();

    lFix.Input.OnKeyPressed(EKeyCode::Space, /*InRepeat*/false);
    lFix.Frame();
    CHECK(lWatcher.bDown);
    CHECK(lWatcher.bPressed);

    lFix.Input.EndFrame();
    lFix.Frame();
    CHECK(lWatcher.bDown);
    CHECK_FALSE(lWatcher.bPressed);

    lFix.Input.EndFrame();
    lFix.Input.OnKeyReleased(EKeyCode::Space);
    lFix.Frame();
    CHECK_FALSE(lWatcher.bDown);
    CHECK(lWatcher.bReleased);
}
