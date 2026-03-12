#pragma once

#include "Subsystem.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax {
    class CoreEngineApp;
}

namespace Opaax
{
    /**
     * @class IEngineSubsystem
     *
     * Meant to be a system handle by the engine
     *
     * Must have the engine ptr as ctor param
     *
     * So this system can have access to the window using the engine ptr,
     * usefull for Input system, base event system for example...
     */
    class OPAAX_API IEngineSubsystem : public Opaax::ISubsystem
    {
    public:
        // =============================================================================
        // CTORS
        // =============================================================================
        IEngineSubsystem():m_EngineApp(nullptr){}
        IEngineSubsystem(CoreEngineApp* InEngineApp):m_EngineApp(InEngineApp){}
        virtual ~IEngineSubsystem() override { ISubsystem::~ISubsystem(); }
        
        IEngineSubsystem(const IEngineSubsystem& other) = default;

        IEngineSubsystem(IEngineSubsystem&& other) noexcept = default;

        IEngineSubsystem& operator=(const IEngineSubsystem& other)
        {
            if (this == &other)
            {
                return *this;
            }
            
            Opaax::ISubsystem::operator =(other);
            m_EngineApp = other.m_EngineApp;
            
            return *this;
        }

        IEngineSubsystem& operator=(IEngineSubsystem&& other) noexcept = default;

        // =============================================================================
        // Functions
        // =============================================================================
    protected:
        void SetEngineApp(CoreEngineApp* InApp) { m_EngineApp = InApp; }

        /*----------------------------- Get - Set -------------------------------*/
    public:
        CoreEngineApp* GetEngineApp() const { return m_EngineApp; }
        CoreEngineApp* GetEngineApp() { return m_EngineApp; }

        // =============================================================================
        // Override
        // =============================================================================
        
        //~Begin ISubsystem interface
    public:
        bool Startup() override = 0;
        void Shutdown() override = 0;
        void Update(double DeltaTime) override {}
        void FixedUpdate(double FixedDeltaTime) override {}
        void Render(double AlphaPhysicStep) override {}
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
    class OPAAX_API EngineSubsystemBase : public IEngineSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        EngineSubsystemBase() = default;
        EngineSubsystemBase(CoreEngineApp* InEngineApp) : IEngineSubsystem(InEngineApp) {}
        virtual ~EngineSubsystemBase() override { IEngineSubsystem::~IEngineSubsystem(); }

        EngineSubsystemBase(const EngineSubsystemBase& Other) = delete;
        EngineSubsystemBase(EngineSubsystemBase&& Other) noexcept = default;

        EngineSubsystemBase& operator=(const EngineSubsystemBase& Other)
        {
            if (this == &Other)
            {
                return *this;
            }
            
            IEngineSubsystem::operator =(Other);
            
            return *this;
        }

        EngineSubsystemBase& operator=(EngineSubsystemBase&& Other) noexcept = default;

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
        void Render(double AlphaPhysicStep) override {}
        //~End IEngineSubsystem Interface
    };

    /**
     * @Class EngineSubsystemMgr
     *
     * Manager that manage Engine Subsystem.
     */
    class OPAAX_API EngineSubsystemMgr : public ISubsystemManager<IEngineSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~EngineSubsystemMgr() override = default;
    };
}
