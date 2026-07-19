#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// AudioManager
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogAudioManager{"AudioManager"};
    
    class OPAAX_API AudioManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(AudioManager)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        AudioManager() = default;
        /***/
        ~AudioManager() override {}
        
        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
        bool Startup() override { return true; }
        void Shutdown() override {}
        //~End EngineSubsystemBase Interface
    };
}