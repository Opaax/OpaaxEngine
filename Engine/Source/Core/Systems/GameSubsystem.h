#pragma once

#include "Subsystem.h"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Log/OpaaxLog.h"

namespace Opaax {
    class OpaaxEvent;
    class CoreEngineApp;
}

namespace Opaax
{
    // =============================================================================
    // IGameSubsystem
    //
    // Game-layer subsystem interface — parallel to IEngineSubsystem. Holds a back
    // pointer to CoreEngineApp so subsystems can reach engine services (World,
    // InputSubsystem, etc.) without singletons.
    //
    // IsPlayOnly() defaults to TRUE here (inverse of IEngineSubsystem). Gameplay
    // systems are PIE-gated by default; opt out by overriding for non-gameplay
    // game subsystems (saving, UI, telemetry, ...).
    // =============================================================================
    class OPAAX_API IGameSubsystem : public Opaax::ISubsystem
    {
        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        IGameSubsystem() : m_EngineApp(nullptr) {}
        IGameSubsystem(CoreEngineApp* InEngineApp) : m_EngineApp(InEngineApp) {}

        virtual ~IGameSubsystem() override                          = default;

        IGameSubsystem(const IGameSubsystem&)                       = delete;
        IGameSubsystem& operator=(const IGameSubsystem&)            = delete;

        IGameSubsystem(IGameSubsystem&& Other) noexcept
            : m_EngineApp(Other.m_EngineApp)
        {
            Other.m_EngineApp = nullptr;
        }

        IGameSubsystem& operator=(IGameSubsystem&& Other) noexcept
        {
            if (this != &Other)
            {
                m_EngineApp       = Other.m_EngineApp;
                Other.m_EngineApp = nullptr;
            }
            return *this;
        }

        // =============================================================================
        // Functions
        // =============================================================================
    protected:
        void SetEngineApp(CoreEngineApp* InApp) { m_EngineApp = InApp; }

    public:
        /**
         * The manager checks this bitmask before calling OnEvent, so subsystems that return EEventCategory_None
         * receive zero event calls — no wasted virtual dispatch.
         * @return return the OR of every EEventCategory this subsystem cares about.
         */
        virtual Uint32 GetEventCategoryFilter() const noexcept  { return EEventCategory_None; }

        /**
         * called only when the event's category intersects the filter.
         * @param Event the event
         * @return Return true to mark bHandled. Does NOT stop dispatch to other subsystems.
         */
        virtual bool   OnEvent(OpaaxEvent& Event)               { return false; }

        /**
         * Play-only subsystems are skipped by GameSubsystemMgr's Update/FixedUpdate
         * loops when the editor is in Editing or Paused state. Default TRUE: gameplay
         * systems are PIE-gated by definition. Override to false for non-gameplay
         * game subsystems (saving, UI, telemetry).
         */
        virtual bool   IsPlayOnly() const noexcept              { return true; }

        /*----------------------------- Get - Set -------------------------------*/
    public:
        CoreEngineApp* GetEngineApp() const noexcept { return m_EngineApp; }

        // =============================================================================
        // Override
        // =============================================================================

        //~Begin ISubsystem interface
    public:
        bool Startup() override = 0;
        void Shutdown() override = 0;
        void Update(double DeltaTime) override {}
        void FixedUpdate(double FixedDeltaTime) override {}
        void Render(double Alpha) override {}
        //~End ISubsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        CoreEngineApp* m_EngineApp;
    };

    // =============================================================================
    // GameSubsystemBase
    //
    // Default-implemented Startup/Shutdown traces so concrete game subsystems can
    // just override Update / OnEvent / etc. Same role as EngineSubsystemBase for
    // the engine layer.
    // =============================================================================
    class OPAAX_API GameSubsystemBase : public IGameSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        GameSubsystemBase() = default;
        explicit GameSubsystemBase(CoreEngineApp* InEngineApp) : IGameSubsystem(InEngineApp) {}
        virtual ~GameSubsystemBase() override = default;

        GameSubsystemBase(const GameSubsystemBase&)                   = delete;
        GameSubsystemBase& operator=(const GameSubsystemBase&)        = delete;
        GameSubsystemBase(GameSubsystemBase&&) noexcept               = default;
        GameSubsystemBase& operator=(GameSubsystemBase&&) noexcept    = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IGameSubsystem Interface
        bool Startup() override
        {
            OPAAX_CORE_INFO("GameSubsystemBase::Startup Is Engine Valid:{0}", GetEngineApp() != nullptr);
            return true;
        }

        void Shutdown() override
        {
            OPAAX_CORE_INFO("GameSubsystemBase::Shutdown Is Engine Valid:{0}", GetEngineApp() != nullptr);
        }

        void Update(double DeltaTime) override {}
        void FixedUpdate(double FixedDeltaTime) override {}
        void Render(double Alpha) override {}
        //~End IGameSubsystem Interface
    };

    // =============================================================================
    // GameSubsystemMgr
    //
    // Manager that owns the game-layer subsystems registered by the game-app's
    // OnInitialize override. CoreEngineApp drives it from Run() (Update/FixedUpdate
    // gated by bAllowPlayOnly) and DispatchEvent (category-filtered).
    // =============================================================================
    class OPAAX_API GameSubsystemMgr : public ISubsystemManager<IGameSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~GameSubsystemMgr() override = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Keep base no-arg overloads available alongside the gated versions below.
        using ISubsystemManager<IGameSubsystem>::UpdateAll;
        using ISubsystemManager<IGameSubsystem>::FixedUpdateAll;

        // Gated variants — skip subsystems whose IsPlayOnly() returns true when
        // bAllowPlayOnly is false (editor in Editing or Paused state).
        void UpdateAll(double DeltaTime, bool bAllowPlayOnly);
        void FixedUpdateAll(double FixedDeltaTime, bool bAllowPlayOnly);

        // PIE-gated like UpdateAll: play-only subsystems are skipped (no OnEvent) when
        // bAllowPlayOnly is false, so gameplay input handlers don't fire in editor edit mode.
        void DispatchEventAll(OpaaxEvent& Event, bool bAllowPlayOnly);
    };
}
