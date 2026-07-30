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

    inline constexpr LogCategory LogWorldManager{"WorldManager"};

    // =============================================================================
    // WorldManager — the engine subsystem that OWNS every World (UniquePtr). Multiple
    //   worlds may coexist (editor + PIE later); one is the "active" world the renderer
    //   draws. Ownership lives here; drivers hold non-owning World* handles.
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
        //Todo: OpaaxStringID
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
        // Override
        // =========================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;

        /**
         * Destroys every remaining world THROUGH DestroyWorld,
         * So each one announces itself while the bus and its subscribers are all still alive.
         * Shutdown() is too late for that, which is exactly why this phase exists.
         */
        void TearDown() override;

        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EngineRegistries*           m_Registries = nullptr; // non-owning; Engine owns them (I5)
        TDynArray<UniquePtr<World>> m_Worlds;
        World*                      m_ActiveWorld = nullptr; // non-owning; points into m_Worlds
        
        // =========================================================================
        // Events
        // =========================================================================
    public:
        FOnWorldCreated       OnWorldCreated;
        FOnWorldDestroyed     OnWorldDestroyed;
        FOnActiveWorldChanged OnActiveWorldChanged;
    };
}
