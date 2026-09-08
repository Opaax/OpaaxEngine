#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
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
    // GameInstanceManager — the engine subsystem that OWNS the GameInstance (0 or 1).
    //   WorldManager's relationship to worlds, one tier up.
    //
    //   REGISTERED BEFORE WorldManager, and that is the design, not an accident:
    //     - UpdateAll walks registration order, so a session subsystem publishes this frame's
    //       answer BEFORE any world subsystem reads it.
    //     - TearDownAll walks it in REVERSE, so worlds are destroyed first and the session
    //       second — "Destroy World then Destroy GameInstance" with no code to enforce it.
    //
    //   It does NOT decide when a game starts. StartGame/EndGame are IEngine verbs the HOST
    //   drives (BO4's "host states policy, engine performs mechanism"): a runtime host brackets
    //   its whole run, the editor's PlayInEditor brackets one PIE cycle, and an editor sitting
    //   in an Edit world has no game at all.
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
         * @param InRegistries The engine's type registries, BORROWED (Engine owns them). Null is
         *                     legal — a bare manager in a test simply creates no subsystems.
         */
        explicit GameInstanceManager(EngineRegistries* InRegistries = nullptr);
        ~GameInstanceManager() override;

        // =========================================================================
        // Game lifetime
        // =========================================================================
    public:
        /**
         * Create the GameInstance and start every registered session subsystem.
         *
         * Called BEFORE the first world exists, which is the whole point: a world subsystem's
         * context is built during CreateWorld, so anything reacting to a world would be too late.
         *
         * REFUSES LOUDLY when a game is already running — a second StartGame is a caller
         * mistake, and silently returning the existing one would hide it.
         *
         * @return true when a game is running as a result of this call.
         */
        bool StartGame();

        /**
         * Destroy the GameInstance: TearDown its subsystems (siblings still alive), then
         * Shutdown, then release.
         *
         * A SILENT no-op when no game is running, deliberately: this is a teardown-path verb
         * and the host calls it unconditionally, so "there was nothing to end" is a normal
         * answer rather than a misconfiguration.
         *
         * Does NOT touch worlds — Engine::EndGame destroys the Play worlds first, so that the
         * order is stated in one place instead of split across two subsystems.
         *
         * @return true when a game was actually ended.
         */
        bool EndGame();

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        /** The running game, or nullptr. Null is a NORMAL state (an editor in an Edit world). */
        GameInstance* GetGameInstance() const noexcept { return m_GameInstance.get(); }

        bool IsGameRunning() const noexcept { return m_GameInstance != nullptr; }

        /**
         * How many games have been started in this run — 1 after the first PIE Play, 2 after the
         * second, and so on.
         *
         * It exists to make the log answer "is the session per-PIE-cycle or once per editor?",
         * which the manager's own start/shutdown lines cannot: THIS type is engine-lifetime and
         * the GameInstance is not, so a reader seeing only "GameInstanceManager started" at boot
         * would reasonably conclude the wrong thing.
         */
        Uint64 GetSessionsStarted() const noexcept { return m_SessionsStarted; }

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup() override;

        /** Ticks the running game's subsystems. Nothing to do when no game is running. */
        void Update(double InDeltaTime) override;

        /** Ends any game the host forgot to end, while every engine sibling is still alive. */
        void TearDown() override;

        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EngineRegistries*      m_Registries = nullptr; // Engine owns
        TUniquePtr<GameInstance> m_GameInstance;

        // The engine-side half of every future GameInstanceContext, resolved ONCE in Startup —
        // the route WorldManager::Startup already uses. Safe during our own Startup because
        // Engine's accessors resolve-from-manager first and the create pass has already built
        // every subsystem; never a lazy self-Startup (F3 / L6).
        WorldManager*           m_Worlds    = nullptr;
        ResourceManager*        m_Resources = nullptr;
        EngineEventBus*         m_Events    = nullptr;
        const InputManager*     m_Input     = nullptr;
        const IPaths*           m_Paths     = nullptr;
        const EngineConfigData* m_Config    = nullptr;

        // Counts games, not managers. See GetSessionsStarted.
        Uint64 m_SessionsStarted = 0;
    };
}
