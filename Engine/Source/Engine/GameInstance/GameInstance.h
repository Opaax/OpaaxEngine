#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/GameInstanceSubsystem.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A game names its own GameInstance class in its project file; without one it runs a GameInstance.
//
//   class MyGame : public Opaax::GameInstance
//   {
//       OPAAX_OBJECT(MyGame, Opaax::GameInstance)
//
//   protected:
//       void OnStart() override { ... }   // its subsystems run; no world exists yet
//       void OnEnd() override   { ... }   // its Play worlds are gone; its subsystems still run
//   };
//   OPAAX_REGISTER_OBJECT(MyGame);
//
//   MyGame.opaaxproj:   "gameInstanceClass": "MyGame"
//
// A session feature (input mapping, saves, a score) is usually a GameInstanceSubsystem instead: every
// game makes them, whatever its class.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    class ObjectClassRegistry;

    inline constexpr LogCategory LogGameInstance{"GameInstance"};

    // =============================================================================
    // GameInstance — one game session, an Object class a game can derive from. Made before the first
    //   world and destroyed after the last Play world, so it survives level changes. Session-scoped
    //   features (input mapping, save, score, ...) are game-instance subsystems.
    // =============================================================================
    class GameInstance : public Object
    {
        OPAAX_OBJECT(GameInstance, Object)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /** Nothing here: the class default object is built with it. A game gets its services in Init. */
        GameInstance();

        /** Shuts the subsystems down if End never ran. */
        ~GameInstance() override;

        /**
         * A new game of the class InClassName names (the project's gameInstanceClass). No name gives a
         * GameInstance; so does a name that is not a concrete GameInstance class of InClasses, with an error.
         */
        static TStrongObjectPtr<GameInstance> Create(OpaaxStringID InClassName, const ObjectClassRegistry& InClasses);

        // =========================================================================
        // Lifetime
        // =========================================================================
    public:
        /**
         * Gives the game its services, once, before Start.
         * @param InContext Copied and kept for the whole game, so subsystems can store a reference to it.
         */
        void Init(const GameInstanceContext& InContext);

        /**
         * Makes one subsystem of each concrete GameInstanceSubsystem class InClasses holds, starts them in
         * tick order, then calls OnStart. Refused before Init, and the second time.
         * @return Number of subsystems made
         */
        Uint64 Start(const ObjectClassRegistry& InClasses);

        /**
         * Calls OnEnd (when Start ran), then tears every subsystem down and shuts it down, in reverse
         * order. Safe to call twice.
         */
        void End();

        // =========================================================================
        // Tick
        // =========================================================================
    public:
        /**
         * Runs before the worlds' update, so session subsystems update first.
         */
        void Update(double InDeltaTime);

        // =========================================================================
        // Hooks
        // =========================================================================
    protected:
        /** The game has started: its subsystems run, no world exists yet. */
        virtual void OnStart() {}

        /** The game is ending: its Play worlds are gone, its subsystems still run. */
        virtual void OnEnd() {}

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        GameInstanceSubsystemMgr&       GetSubsystems()       noexcept { return m_Subsystems; }
        const GameInstanceSubsystemMgr& GetSubsystems() const noexcept { return m_Subsystems; }

        /** Stable for the whole game, from Init. */
        GameInstanceContext&       GetContext()       noexcept { return *m_Context; }
        const GameInstanceContext& GetContext() const noexcept { return *m_Context; }

        Uint64 GetSubsystemCount() const noexcept
        {
            return static_cast<Uint64>(m_Subsystems.GetSystems().size());
        }

        /** True between Start and End. */
        bool IsRunning() const noexcept { return m_bStarted && !m_bEnded; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        // On the heap: subsystems keep a reference to it.
        TUniquePtr<GameInstanceContext> m_Context;

        GameInstanceSubsystemMgr m_Subsystems;

        bool m_bStarted = false;
        bool m_bEnded   = false;
    };
}
