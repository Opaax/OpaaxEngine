#pragma once

#include "IAppService.h"

namespace Opaax
{
    class EngineEventBus;
    class ResourceManager;
    class WorldManager;
    class IRenderTarget;
    class DebugDraw;

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
         * Show the rendered frame on screen — swap the OS window's backbuffer. Host-driven, called
         * once after the frame's render (F2 present-split). Named for what it presents: ONLY the
         * backbuffer is ever shown; an offscreen primary target (editor viewport) is never presented.
         */
        virtual void PresentBackbuffer()                = 0;

        /**
         * Redirect the world render into InTarget instead of the window backbuffer; nullptr restores
         * the backbuffer (the runtime default — Sandbox never calls this). Non-owning: the caller owns
         * the target's lifetime and must clear it (pass nullptr) before the target dies. The editor
         * points this at its ViewportPanel's offscreen FBO so the world lands in a texture (D2).
         */
        virtual void SetPrimaryRenderTarget(IRenderTarget* InTarget) = 0;

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
         * Phase 1 of stopping: the frame loop has ended, but NOTHING is destroyed yet —
         * subsystems, services, window and GPU context are all still alive.
         *
         * Subsystems release anything that needs a live sibling here, because Shutdown()
         * cannot offer that guarantee. Called by the host at the end of RunApplication,
         * before ShutdownApplication.
         */
        virtual void TearDown()                         = 0;

        /**
         * Phase 2: stop + destroy the engine subsystems (reverse of startup). Idempotent.
         *
         * Runs during locator teardown, AFTER TearDown(). Siblings may already be gone by
         * the time a given subsystem's Shutdown runs — do not reach out of yourself here.
         */
        virtual void Shutdown()                         = 0;
        
        // End Lifecycle — driven by the application host
        // =============================================================================

        // =============================================================================
        // Foundation subsystems — exposed directly
    public:
        virtual ResourceManager& GetResources() = 0;
        virtual EngineEventBus& GetEngineEventBus() = 0;
        virtual WorldManager& GetWorldManager() = 0;

        /**
         * The per-frame debug line queue (D10). Enqueue from anywhere in the frame BEFORE the render
         * that should show it — the renderer drains and clears it every frame, so a line must be
         * re-submitted each frame it stays visible. Serves editor overlays and dev builds of the
         * game alike; the engine has no idea which one is calling.
         */
        virtual DebugDraw& GetDebugDraw() = 0;

        // End Foundation subsystems
        // =============================================================================
        
    public:
        //----- null object ----------------------------------------------------
        static IEngine& Null();
    };
}
