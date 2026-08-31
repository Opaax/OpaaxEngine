#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "World/World.h"
#include "World/WorldEvents.h"

namespace Opaax
{
    class EngineRegistries;
    class ResourceManager;
    class EngineEventBus;
    class DebugDraw;
    class IPaths;

    inline constexpr LogCategory LogWorldManager{"WorldManager"};

    // =============================================================================
    // WorldManager — the engine subsystem that OWNS every World (TUniquePtr). Multiple
    //   worlds may coexist (editor + PIE later); one is the "active" world the renderer
    //   draws. Ownership lives here; drivers hold non-owning World* handles.
    //
    //   CloneWorld is PIE: it snapshots one world into another running in a different mode,
    //   leaving the source alive and untouched so Stop is just "activate the source again".
    //
    //   It creates NO world of its own (BO4). Starting a subsystem is infrastructure;
    //   choosing which world to open is content, and that happens last: the host NAMES it
    //   (OpaaxApplication::GetStartupWorldSpec) and Engine::FinishStartup creates it. There is
    //   legitimately no active world between Startup and that call, and every consumer handles it.
    // =============================================================================
    class OPAAX_API WorldManager final : public EngineSubsystemBase
    {
        // =========================================================================
        // Base Implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(WorldManager)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /**
         * @param InRegistries The engine's type registries, BORROWED (Engine owns them). Sealed
         *                     here on the way to the first world. Null is legal — a bare manager
         *                     in a test simply has nothing to seal.
         */
        explicit WorldManager(EngineRegistries* InRegistries = nullptr);
        ~WorldManager() override = default;

        // =========================================================================
        // Function
        // =========================================================================

        // =========================================================================
        // World Lifetime
    public:
        /**
         * Create a world and take ownership of it. Does NOT activate it — the caller decides
         * (SetActiveWorld), because a PIE clone is created before it becomes active and the
         * source world stays alive throughout.
         *
         * The FIRST call seals the engine registries: nothing may register a component or world
         * subsystem type once a world exists to have been built without it (BO4).
         *
         * @param InName
         * @param InMode What the world is for. Fixed at construction (see EWorldMode).
         */
        World* CreateWorld(OpaaxString InName = "World", EWorldMode InMode = EWorldMode::Play);

        /**
         *
         * @param InSource The world to copy. Read-only; it need not be active.
         * @param InMode   What the CLONE is for. Never inherited from the source.
         * @return The clone, NOT activated (the caller decides), or null with no registries to
         *         capture through — an empty "clone" would silently diverge.
         */
        World* CloneWorld(const World& InSource, EWorldMode InMode);

        void   DestroyWorld(World* InWorld);
        // End World Lifetime
        // =========================================================================

        // =========================================================================
        // Getters
    public:
        World* GetActiveWorld() const noexcept { return m_ActiveWorld; }
        bool   SetActiveWorld(World* InWorld) noexcept;

        Uint64 GetWorldCount() const noexcept { return static_cast<Uint64>(m_Worlds.size()); }

        /** The engine's registries, borrowed. Null only for a bare manager in a test. */
        EngineRegistries* GetRegistries() const noexcept { return m_Registries; }

        // End Getters
        // =========================================================================

        // =========================================================================
        // Tick gate — PIE pause / step
    public:
        /**
         * Suspend the active world's tick. The editor's Pause; nothing else sets it.
         *
         * Deliberately mode-agnostic: an Edit world can be paused too. The rule is about the
         * TICK, not about what a world is for, and a gate that inspected the mode would need a
         * reason no caller has.
         */
        void SetPaused(bool InPaused) noexcept { m_bPaused = InPaused; }
        bool IsPaused() const noexcept         { return m_bPaused; }

        /**
         * Tick exactly ONE more frame, then stay paused — the editor's Step.
         *
         * A frame, not an Update: Engine::Loop runs Update once and FixedUpdate 0..N times, so
         * stepping only the Update would advance the world while starving the fixed step. The
         * request is consumed in Update and both hooks read the same per-frame decision.
         */
        void RequestStep() noexcept { m_bStepRequested = true; }

        /** Whether the current frame is ticking the world — the decision Update took. */
        bool IsTickingThisFrame() const noexcept { return m_bTickThisFrame; }

        // End Tick gate
        // =========================================================================

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;

        /**
         * Tick the ACTIVE world's subsystems. Only the active one runs: a PIE clone and the edit
         * world coexist, and exactly one of them is simulating.
         */
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;

        // NOTE: no Render override, deliberately (Editor.md §3). A world subsystem draws by
        // submitting to DebugDraw from its Update — immediate mode, drained every frame by the
        // renderer (F4). Giving subsystems a Render hook would create a second, competing draw
        // path into a frame the RendererManager already owns.

        /**
         * Destroys every remaining world THROUGH DestroyWorld,
         * So each one announces itself while the bus and its subscribers are all still alive.
         * Shutdown() is too late for that, which is exactly why this phase exists.
         */
        void TearDown() override;

        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /**
         * Build InWorld's context, create the subsystem candidates it qualifies for, and start
         * them. Called by CreateWorld, so it runs for a PIE clone exactly as for the first world.
         */
        void CreateSubsystemsFor(World& InWorld);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EngineRegistries*               m_Registries = nullptr; // Engine owns
        TDynArray<TUniquePtr<World>>    m_Worlds;
        World*                          m_ActiveWorld = nullptr; // non-owning;
        
        ResourceManager* m_Resources = nullptr;
        EngineEventBus*  m_Events    = nullptr;
        DebugDraw*       m_Debug     = nullptr;

        // Resolves a level manifest's asset-relative map paths. Cached with the others in Startup;
        // it is an APP service rather than an engine subsystem, which is the only difference.
        const IPaths*    m_Paths     = nullptr;

        // ④ — resolved in Startup like the siblings above, put into every WorldContext, and used
        // for this manager's own tick scope. Null in a bare test manager.
        FrameProfiler*   m_Profiler  = nullptr;
        
        bool m_bPaused        = false;
        bool m_bStepRequested = false;
        bool m_bTickThisFrame = true;
        
        // =========================================================================
        // Events
        // =========================================================================
    public:
        FOnWorldCreated       OnWorldCreated;
        FOnWorldDestroyed     OnWorldDestroyed;
        FOnActiveWorldChanged OnActiveWorldChanged;
    };
}
