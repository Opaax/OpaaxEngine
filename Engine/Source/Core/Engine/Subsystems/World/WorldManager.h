#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// WorldManager
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogWorldManager{"WorldManager"};
    
    class OPAAX_API WorldManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(WorldManager)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        WorldManager() = default;
        /***/
        ~WorldManager() override {}
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
        bool Startup() override { return true; }
        void Shutdown() override {}
        //~End EngineSubsystemBase Interface
    };
}