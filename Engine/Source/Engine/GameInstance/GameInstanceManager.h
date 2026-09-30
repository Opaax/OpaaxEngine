#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"

namespace Opaax
{
    class EngineRegistries;
    class GameInstance;
    class WorldManager;
    class ResourceManager;
    class EngineEventBus;
    class InputManager;
    class IPaths;
    struct EngineConfigData;

    inline constexpr LogCategory LogGameInstanceManager{"GameInstanceManager"};

    // =============================================================================
    // GameInstanceManager — engine subsystem that owns the GameInstance (0 or 1).
    //   Registered before WorldManager: sessions update before worlds, and worlds are
    //   destroyed before the session. The host decides when a game starts and ends.
    // =============================================================================
    class OPAAX_API GameInstanceManager final : public EngineSubsystemBase
    {
        // =========================================================================
        // Base Implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(GameInstanceManager)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /**
         * @param InRegistries The engine's registries (owned by Engine). May be null in tests.
         */
        explicit GameInstanceManager(EngineRegistries* InRegistries = nullptr);
        ~GameInstanceManager() override;

        // =========================================================================
        // Game lifetime
        // =========================================================================
    public:
        /**
         * Creates the GameInstance and starts its subsystems. Call before the first world exists.
         * Fails with an error if a game is already running.
         * @return True if a game is now running
         */
        bool StartGame();

        /**
         * Destroys the GameInstance (TearDown, Shutdown, release). Does nothing if no game is running.
         * Does not touch worlds: Engine::EndGame destroys the Play worlds first.
         * @return True if a game was ended
         */
        bool EndGame();

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        /** The running game, or nullptr (normal in an Edit world). */
        GameInstance* GetGameInstance() const noexcept { return m_GameInstance.get(); }

        bool IsGameRunning() const noexcept { return m_GameInstance != nullptr; }

        /**
         * Number of games started in this run (1 after the first Play, 2 after the second, ...).
         */
        Uint64 GetSessionsStarted() const noexcept { return m_SessionsStarted; }

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup() override;

        /** Ticks the running game's subsystems, if any. */
        void Update(double InDeltaTime) override;

        /** Ends a game the host forgot to end. */
        void TearDown() override;

        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EngineRegistries*      m_Registries = nullptr; // owned by Engine
        TUniquePtr<GameInstance> m_GameInstance;

        // Used to build every GameInstanceContext. Resolved once in Startup.
        WorldManager*           m_Worlds    = nullptr;
        ResourceManager*        m_Resources = nullptr;
        EngineEventBus*         m_Events    = nullptr;
        const InputManager*     m_Input     = nullptr;
        const IPaths*           m_Paths     = nullptr;
        const EngineConfigData* m_Config    = nullptr;

        // Number of games started. See GetSessionsStarted.
        Uint64 m_SessionsStarted = 0;
    };
}
