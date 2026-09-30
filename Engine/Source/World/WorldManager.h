#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"
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
    struct EngineConfigData;
    class InputManager;
    class GameInstanceManager;

    inline constexpr LogCategory LogWorldManager{"WorldManager"};

    // =============================================================================
    // WorldManager — engine subsystem that owns every World. Several can coexist (edit world
    //   and its Play copy); one is active (ticked and drawn).
    //   CloneWorld makes the Play copy; the source is untouched, so Stop just re-activates it.
    //   Creates no world itself: Engine::FinishStartup creates the startup world.
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
         * @param InRegistries The engine's registries (owned by Engine), sealed at the first world.
         *                     May be null in tests.
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
         * Creates and owns a world. Does not activate it (call SetActiveWorld).
         * The first call seals the engine registries.
         * @param InMode What the world is for (cannot change)
         */
        World* CreateWorld(OpaaxString InName = "World", EWorldMode InMode = EWorldMode::Play);

        /**
         * Copies InSource into a new world (Play In Editor).
         * @param InSource The world to copy (read-only; need not be active)
         * @param InMode   What the clone is for
         * @return The clone, not activated; null without registries
         */
        World* CloneWorld(const World& InSource, EWorldMode InMode);

        void   DestroyWorld(World* InWorld);

        /**
         * Destroys every world in InMode (IEngine::EndGame destroys the Play worlds; the Edit world stays).
         * @return Number of worlds destroyed
         */
        Uint64 DestroyWorldsOfMode(EWorldMode InMode);

        /** @return Number of worlds in InMode */
        Uint64 CountWorldsOfMode(EWorldMode InMode) const noexcept;
        // End World Lifetime
        // =========================================================================

        // =========================================================================
        // Getters
    public:
        World* GetActiveWorld() const noexcept { return m_ActiveWorld; }
        bool   SetActiveWorld(World* InWorld) noexcept;

        Uint64 GetWorldCount() const noexcept { return static_cast<Uint64>(m_Worlds.size()); }

        /** The engine's registries. Null only in tests. */
        EngineRegistries* GetRegistries() const noexcept { return m_Registries; }

        // End Getters
        // =========================================================================

        // =========================================================================
        // Tick gate — Play In Editor pause / step
    public:
        /**
         * Pauses the active world's tick (editor Pause). Works for any mode.
         */
        void SetPaused(bool InPaused) noexcept { m_bPaused = InPaused; }
        bool IsPaused() const noexcept         { return m_bPaused; }

        /**
         * Ticks one more frame, then stays paused (editor Step). Covers Update and FixedUpdate.
         */
        void RequestStep() noexcept { m_bStepRequested = true; }

        /** Whether the world ticks this frame (decided in Update). */
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
         * Ticks the active world's subsystems only.
         */
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;

        // No Render override: world subsystems draw by submitting to DebugDraw.

        /**
         * Destroys every world through DestroyWorld, while the event bus and its subscribers are alive.
         */
        void TearDown() override;

        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /**
         * Builds InWorld's context, creates the subsystems it qualifies for, and starts them.
         */
        void CreateSubsystemsFor(World& InWorld);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EngineRegistries*               m_Registries = nullptr; // owned by Engine
        TDynArray<TUniquePtr<World>>    m_Worlds;
        World*                          m_ActiveWorld = nullptr; // not owned
        
        ResourceManager* m_Resources = nullptr;
        EngineEventBus*  m_Events    = nullptr;
        DebugDraw*       m_Debug     = nullptr;

        // Resolves the manifest's asset-relative map paths.
        const IPaths*    m_Paths     = nullptr;

        // Engine config, given to every WorldContext (read-only).
        const EngineConfigData* m_Config = nullptr;

        // This frame's input, given to every WorldContext.
        const InputManager* m_Input = nullptr;

        // The running game, if any. Looked up per world (games come and go across Play sessions).
        GameInstanceManager* m_GameInstances = nullptr;

        
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
