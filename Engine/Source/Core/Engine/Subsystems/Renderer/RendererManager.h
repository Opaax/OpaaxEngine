#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// RendererManager 
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogRendererManager{"RendererManager"};
    
    class OPAAX_API RendererManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(RendererManager)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        RendererManager() = default;
        /***/
        ~RendererManager() override {}
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
        bool Startup() override { return true; }
        void Shutdown() override {}
        //~End EngineSubsystemBase Interface
    };
}