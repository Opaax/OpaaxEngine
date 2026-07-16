#pragma once
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"
#include "Core/Events/EventBus.h"

namespace Opaax
{
    inline constexpr LogCategory LogEngineEventBus{"EngineEventBus"};

    class OPAAX_API EngineEventBus final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(EngineEventBus)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        EngineEventBus() = default;
        /***/
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
        //~Begin EngineSubsystemBase Interface
        bool Startup() override;
        void Shutdown() override;
        //~End EngineSubsystemBase Interface
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        EventBus m_EventBus;
    };
}
