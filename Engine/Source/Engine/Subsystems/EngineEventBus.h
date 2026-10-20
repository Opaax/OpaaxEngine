#pragma once
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Core/Events/EventBus.h"

namespace Opaax
{
    inline constexpr LogCategory LogEngineEventBus{"EngineEventBus"};

    class EngineEventBus final : public EngineSubsystem
    {
        OPAAX_OBJECT(EngineEventBus, EngineSubsystem)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        EngineEventBus() = default;
        ~EngineEventBus() override {}
        
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        
        // =============================================================================
        // Getter
        const EventBus& GetEventBus() const { return m_EventBus; }
        EventBus& GetEventBus() { return m_EventBus; }
        // Getter
        // =============================================================================
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Subsystem interface
        bool Startup() override;
        void Shutdown() override;
        //~End Subsystem interface
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        EventBus m_EventBus;
    };
}
