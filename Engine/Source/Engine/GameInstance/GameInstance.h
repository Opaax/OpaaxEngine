#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"

namespace Opaax
{
    class GameInstanceSubsystemRegistry;

    inline constexpr LogCategory LogGameInstance{"GameInstance"};

    // =============================================================================
    // GameInstance — one game session. Created by StartGame BEFORE any world exists and
    //   destroyed by EndGame after the last Play world is gone, so it OUTLIVES every world
    //   it plays through: level travel destroys one world and creates another, and the
    //   session is what does not change across that.
    //
    //   It holds SUBSYSTEMS and a CONTEXT and nothing else. Everything session-scoped —
    //   input mapping today, save/score later — is a tenant, never a member of this class.
    //   That is what keeps "the game instance knows about a lot of things" from turning it
    //   into a bag: it knows about a lot of things because its tenants do.
    //
    //   NO ShouldCreate filter on the candidates (unlike a World, WS2): Edit and Play worlds
    //   coexist and want different subsystem sets, but there is only ever one kind of game.
    // =============================================================================
    class OPAAX_API GameInstance
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /**
         * @param InContext COPIED into a stable heap slot owned by this instance, so a
         *   subsystem may store GameInstanceContext& for the whole game (WS4's reasoning).
         */
        explicit GameInstance(const GameInstanceContext& InContext);
        ~GameInstance();

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        GameInstance(const GameInstance&)            = delete;
        GameInstance& operator=(const GameInstance&) = delete;
        GameInstance(GameInstance&&)                 = delete;
        GameInstance& operator=(GameInstance&&)      = delete;

        // =========================================================================
        // Lifetime
        // =========================================================================
    public:
        /**
         * Create every candidate in InRegistry in registration order, then start them all.
         *
         * One StartupAll for the whole set, so the create pass finishes before any Startup
         * runs and a subsystem can reach a sibling during its own (F3, one tier over).
         *
         * @return how many subsystems were created.
         */
        Uint64 StartSubsystems(const GameInstanceSubsystemRegistry& InRegistry);

        /**
         * LC TearDown for every subsystem, reverse order — the phase in which every engine
         * sibling a context points at is still alive. Idempotent (LC3).
         */
        void TearDownSubsystems();

        /** LC Shutdown for every subsystem, reverse order. Idempotent (LC3). */
        void ShutdownSubsystems();

        // =========================================================================
        // Tick
        // =========================================================================
    public:
        /**
         * Runs BEFORE WorldManager's, because GameInstanceManager is registered before it and
         * ISubsystemManager::UpdateAll walks registration order. That is what lets a session
         * subsystem publish this frame's answer before any world subsystem reads it.
         */
        void Update(double InDeltaTime);

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        GameInstanceSubsystemMgr&       GetSubsystems()       noexcept { return m_Subsystems; }
        const GameInstanceSubsystemMgr& GetSubsystems() const noexcept { return m_Subsystems; }

        /** Stable for the whole game — the address a subsystem's stored reference points at. */
        GameInstanceContext&       GetContext()       noexcept { return *m_Context; }
        const GameInstanceContext& GetContext() const noexcept { return *m_Context; }

        Uint64 GetSubsystemCount() const noexcept
        {
            return static_cast<Uint64>(m_Subsystems.GetSystems().size());
        }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        // Heap, not by value: a subsystem stores GameInstanceContext&, and only a stable
        // address makes that safe for the game's whole life.
        TUniquePtr<GameInstanceContext> m_Context;

        GameInstanceSubsystemMgr m_Subsystems;

        bool m_bTornDown  = false;
        bool m_bShutDown  = false;
    };
}
