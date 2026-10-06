#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"

namespace Opaax
{
    class GameInstanceSubsystemRegistry;

    inline constexpr LogCategory LogGameInstance{"GameInstance"};

    // =============================================================================
    // GameInstance — one game session. Created before the first world and destroyed after the
    //   last Play world, so it survives level changes. Session-scoped features (input mapping,
    //   save, score, ...) are game-instance subsystems.
    // =============================================================================
    class GameInstance
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /**
         * @param InContext Copied and kept for the whole game, so subsystems can store a reference to it.
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
         * Creates every registered subsystem in order, then starts them all.
         * @return Number of subsystems created
         */
        Uint64 StartSubsystems(const GameInstanceSubsystemRegistry& InRegistry);

        /**
         * First shutdown step, in reverse order. Safe to call twice.
         */
        void TearDownSubsystems();

        /** Shuts every subsystem down, in reverse order. Safe to call twice. */
        void ShutdownSubsystems();

        // =========================================================================
        // Tick
        // =========================================================================
    public:
        /**
         * Runs before the worlds' update, so session subsystems update first.
         */
        void Update(double InDeltaTime);

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        GameInstanceSubsystemMgr&       GetSubsystems()       noexcept { return m_Subsystems; }
        const GameInstanceSubsystemMgr& GetSubsystems() const noexcept { return m_Subsystems; }

        /** Stable for the whole game. */
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
        // On the heap: subsystems keep a reference to it.
        TUniquePtr<GameInstanceContext> m_Context;

        GameInstanceSubsystemMgr m_Subsystems;

        bool m_bTornDown  = false;
        bool m_bShutDown  = false;
    };
}
