#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// InputManager
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogInputManager{"InputManager"};
    
    class OPAAX_API InputManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(InputManager)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        InputManager() = default;
        /***/
        ~InputManager() override {}
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
        bool Startup() override { return true; }
        void Shutdown() override {}
        //~End EngineSubsystemBase Interface
    };
}