#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Object/ObjectPtr.h"

namespace Opaax
{
    class EngineRegistries;
    class WorldManager;
    class ResourceManager;
    class EngineEventBus;
    class InputManager;
    class IPaths;
    struct EngineConfigData;

    inline constexpr LogCategory LogGameInstanceManager{"GameInstanceManager"};

    // =============================================================================
    // GameInstanceManager — engine subsystem that owns the GameInstance (0 or 1), of the class the
    //   project names. Registered before WorldManager: sessions update before worlds, and worlds are
    //   destroyed before the session. The host decides when a game starts and ends.
    // =============================================================================
    class GameInstanceManager final : public EngineSubsystem
    {
        OPAAX_OBJECT(GameInstanceManager, EngineSubsystem)

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
         * Makes the GameInstance (of GetGameInstanceClass) and starts it. Call before the first world
         * exists. Fails with an error if a game is already running.
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
        GameInstance* GetGameInstance() const noexcept { return m_GameInstance.Get(); }

        bool IsGameRunning() const noexcept { return m_GameInstance.IsValid(); }

        /**
         * The class the next game is made of, by name: the project's gameInstanceClass, read at Startup.
         * None gives a GameInstance.
         */
        OpaaxStringID GetGameInstanceClass() const noexcept { return m_GameInstanceClass; }
        void          SetGameInstanceClass(const OpaaxStringID InClassName) noexcept { m_GameInstanceClass = InClassName; }

        /**
         * Number of games started in this run (1 after the first Play, 2 after the second, ...).
         */
        Uint64 GetSessionsStarted() const noexcept { return m_SessionsStarted; }

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Subsystem interface
    public:
        bool Startup() override;

        /** Ticks the running game's subsystems, if any. */
        void Update(double InDeltaTime) override;

        /** Ends a game the host forgot to end. */
        void TearDown() override;

        void Shutdown() override;
        //~End Subsystem interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EngineRegistries*              m_Registries = nullptr; // owned by Engine
        TStrongObjectPtr<GameInstance> m_GameInstance;

        // See GetGameInstanceClass.
        OpaaxStringID m_GameInstanceClass;

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
