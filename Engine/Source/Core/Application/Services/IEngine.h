#pragma once

#include "IAppService.h"

namespace Opaax
{
    class EngineEventBus;
    class ResourceManager;

    // =============================================================================
    // IEngine — the engine, exposed as an application service. Owns the engine
    // subsystems (Resources first; Render/Input/World/Physics later) and the
    // per-frame tick that pumps them. The application host drives the lifecycle;
    // Get<IEngine>() NEVER returns null (the null object is inert).
    // =============================================================================
    class OPAAX_API IEngine : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IEngine)
        
        // =============================================================================
        // Functions
        // =============================================================================

        // =============================================================================
        // Lifecycle — driven by the application host
    public:
        /**
         * Start the engine subsystems. 
         * Safe to call once
         * @return 
         */
        virtual bool Startup()                          = 0;

        /**
         * Engine Loop
         */
        virtual void Loop()                             = 0;
        
        /**
         * Called once per rendered frame.
         * 
         * Before Physic
         * Before Render
         * @param InDeltaTime 
         */
        virtual void Update(double InDeltaTime)         = 0;

        /**
         * Can be call multiple time by frame
         * After Update
         * Before Render
         * @param InFixedDeltaTime 
         */
        virtual void FixedUpdate(double InFixedDeltaTime) = 0;

        /**
         * Called once per rendered frame.
         * 
         * After Update
         * After Physic
         * @param InAlphaPhysicStep 
         */
        virtual void Render(double InAlphaPhysicStep)   = 0;
        
        /**
         * Stop + tear down the engine subsystems (reverse of startup). Idempotent.
         */
        virtual void Shutdown()                         = 0;
        
        // End Lifecycle — driven by the application host
        // =============================================================================

        // =============================================================================
        // Foundation subsystems — exposed directly
    public:
        virtual ResourceManager& GetResources() = 0;
        virtual EngineEventBus& GetEngineEventBus() = 0;

        // End Foundation subsystems
        // =============================================================================
        
    public:
        //----- null object ----------------------------------------------------
        static IEngine& Null();
    };
}
