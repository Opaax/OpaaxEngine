#pragma once

#include "Core/Systems/Subsystem.h"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    class OpaaxEvent;
    class CoreEngineApp;
}

namespace Opaax
{
    // =============================================================================
    // IEngineSubsystem — marker interface for engine-owned subsystems.
    //   Lifetime = engine (up on engine start, down on engine stop). Managed by
    //   EngineSubsystemMgr. No CoreEngineApp coupling — the service-locator world
    //   reaches shared facilities through OpaaxApplication::GetAppService<T>().
    // =============================================================================
    class OPAAX_API IEngineSubsystem : public Opaax::ISubsystem
    {
    };

    // =============================================================================
    // EngineSubsystemBase — ctor/dtor boilerplate for concrete engine subsystems.
    //   Leaves Startup()/Shutdown() pure (each concrete implements them) and inherits
    //   the no-op Update/FixedUpdate/Render from ISubsystem. Stamp the concrete with
    //   OPAAX_SUBSYSTEM_TYPE(ClassName) for GetTypeID/StaticTypeID.
    // =============================================================================
    class OPAAX_API EngineSubsystemBase : public Opaax::IEngineSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        EngineSubsystemBase()          = default;
        ~EngineSubsystemBase() override = default;

        // Heap-owned via UniquePtr in the manager — never copied or moved.
        EngineSubsystemBase(const EngineSubsystemBase&)            = delete;
        EngineSubsystemBase& operator=(const EngineSubsystemBase&) = delete;
        EngineSubsystemBase(EngineSubsystemBase&&)                 = delete;
        EngineSubsystemBase& operator=(EngineSubsystemBase&&)      = delete;
    };

    // =============================================================================
    // EngineSubsystemMgr — owns + drives the engine subsystem list. Inherits
    //   Register/Startup/Update/FixedUpdate/Render/Shutdown from ISubsystemManager
    //   (registration order; reverse order for shutdown).
    // =============================================================================
    class OPAAX_API EngineSubsystemMgr : public ISubsystemManager<IEngineSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~EngineSubsystemMgr() override = default;
    };
    
    // =============================================================================
    // Old
    // =============================================================================
    
    /**
     * @class IEngineSubsystemOld
     *
     * Meant to be a system handle by the engine
     *
     * Must have the engine ptr as ctor param
     *
     * So this system can have access to the window using the engine ptr,
     * usefull for Input system, base event system for example...
     */
    class OPAAX_API IEngineSubsystemOld : public Opaax::ISubsystem
    {
    public:
        // =============================================================================
        // CTORS
        // =============================================================================
        IEngineSubsystemOld():m_EngineApp(nullptr){}
        IEngineSubsystemOld(CoreEngineApp* InEngineApp):m_EngineApp(InEngineApp){}
        
        virtual ~IEngineSubsystemOld() override                        = default;
        
        IEngineSubsystemOld(const IEngineSubsystemOld&)                   = delete;
        IEngineSubsystemOld& operator=(const IEngineSubsystemOld&)        = delete;
        
        IEngineSubsystemOld(IEngineSubsystemOld&& Other) noexcept
            : m_EngineApp(Other.m_EngineApp)
        {
            Other.m_EngineApp = nullptr;
        }
 
        IEngineSubsystemOld& operator=(IEngineSubsystemOld&& Other) noexcept
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
         * Play-only subsystems are skipped by EngineSubsystemMgr's Update/FixedUpdate
         * loops when the editor is in Editing or Paused state. Default false: subsystem
         * always ticks. Override to true for gameplay-only systems (physics, AI, scripts).
         */
        virtual bool   IsPlayOnly() const noexcept              { return false; }

        /**
         * Per-play-session edges, fired by CoreEngineApp on the play-state transition
         * (editor Play/Stop; once at startup in release). OnPlayBegin runs before the
         * first gameplay tick of a session, OnPlayEnd after the last — the place to build
         * and tear down per-play state (e.g. physics bodies) that must not leak across
         * sessions. Called regardless of IsPlayOnly. Default no-op.
         */
        virtual void   OnPlayBegin()                            {}
        virtual void   OnPlayEnd()                              {}

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

    /**
     * @EngineSubsystemBase
     * Base Engine subsystem that implement ctor/dtor
     */
    class OPAAX_API EngineSubsystemBaseOld : public IEngineSubsystemOld
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        EngineSubsystemBaseOld() = default;
        explicit EngineSubsystemBaseOld(CoreEngineApp* InEngineApp) : IEngineSubsystemOld(InEngineApp) {}
        virtual ~EngineSubsystemBaseOld() override = default;

        EngineSubsystemBaseOld(const EngineSubsystemBaseOld&)                   = delete;
        EngineSubsystemBaseOld& operator=(const EngineSubsystemBaseOld&)        = delete;
        EngineSubsystemBaseOld(EngineSubsystemBaseOld&&) noexcept               = default;
        EngineSubsystemBaseOld& operator=(EngineSubsystemBaseOld&&) noexcept    = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEngineSubsystem Interface
        bool Startup() override
        {
            OPAAX_CORE_INFO("EngineSubsystemBase::Startup Is Engine Valid:{0}", GetEngineApp() != nullptr);
            return true;
        }
        
        void Shutdown() override
        {
            OPAAX_CORE_INFO("EngineSubsystemBase::Shutdown Is Engine Valid:{0}", GetEngineApp() != nullptr);
        }
        
        void Update(double DeltaTime) override {}
        void FixedUpdate(double FixedDeltaTime) override {}
        void Render(double Alpha) override {}
        //~End IEngineSubsystem Interface
    };

    /**
     * @Class EngineSubsystemMgr
     *
     * Manager that manage Engine Subsystem.
     */
    class OPAAX_API EngineSubsystemMgrOld : public ISubsystemManager<IEngineSubsystemOld>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~EngineSubsystemMgrOld() override = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Keep base no-arg overloads available alongside the gated versions below.
        using ISubsystemManager<IEngineSubsystemOld>::UpdateAll;
        using ISubsystemManager<IEngineSubsystemOld>::FixedUpdateAll;

        // Gated variants — skip subsystems whose IsPlayOnly() returns true when
        // bAllowPlayOnly is false (editor in Editing or Paused state).
        void UpdateAll(double DeltaTime, bool bAllowPlayOnly);
        void FixedUpdateAll(double FixedDeltaTime, bool bAllowPlayOnly);

        // Broadcast the per-play-session edges to every subsystem (registration order
        // for Begin, reverse for End — mirrors Startup/Shutdown symmetry).
        void OnPlayBeginAll();
        void OnPlayEndAll();

        void DispatchEventAll(OpaaxEvent& Event);
    };
}
