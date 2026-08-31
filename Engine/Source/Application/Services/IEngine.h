#pragma once

#include "IAppService.h"
#include "Core/OpaaxTypes.h"   // TUniquePtr (CreateFramebuffer's return)
#include "Application/WorldSpec.h"   // WorldSpec (by value across the seam)

namespace Opaax
{
    class EngineEventBus;
    class ResourceManager;
    class WorldManager;
    class InputManager;
    class World;
    class IFramebuffer;
    class IRenderTarget;
    class ITexture2D;
    class DebugDraw;
    class EngineRegistries;
    struct FramebufferSpec;

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
        virtual bool Startup() = 0;

        /**
         * @param InSpec Which world to open, in which mode.
         * @return The created world, already active. Null only if the engine is not started.
         */
        virtual World* FinishStartup(const WorldSpec& InSpec) = 0;

        /**
         * Open InSpec's level into a NEW world, activate it, and destroy the one it replaces.
         *
         * The single path a level is opened by — `FinishStartup` is this call the first time, not
         * a second route that has to be kept in step with it. An EMPTY `LevelPath` is a supported
         * answer, not a misconfiguration (see WorldSpec): it gives a NullLevel world with an empty
         * Level, which is what editing a map that belongs to no level needs.
         *
         * The new world is activated BEFORE the old one is destroyed, so no frame ever runs
         * without an active world.
         *
         * @return The new world, already active, or null if the engine is not started.
         */
        virtual World* OpenLevel(const WorldSpec& InSpec) = 0;

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
         * the backbuffer. Non-owning: the caller owns the target's lifetime and must clear it (pass nullptr) before the target dies. 
         * The editor points this at its ViewportPanel's offscreen FBO so the world lands in a texture.
         */
        virtual void SetPrimaryRenderTarget(IRenderTarget* InTarget) = 0;
        
        /**
         * The CALLER owns the result and must release it while the engine — and its GPU context — is still alive. 
         * @param InSpec 
         * @return Valid only after Startup; returns nullptr before it, or if the device is gone.
         */
        virtual TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec) = 0;

        /**
         * A GPU texture from pixels the caller already decoded (F2a — only the device creates one).
         * The route a resource takes to the GPU: TextureResource::Initialize runs on the pump and
         * has no device of its own, and caching one would outlive it.
         *
         * @param InPixels Tightly packed, InWidth * InHeight * InChannels bytes. Borrowed — the
         *   caller may free it as soon as this returns.
         * @param InChannels 4 = RGBA8, 3 = RGB8, 1 = R8 coverage.
         * @return CALLER-OWNED, and released while the engine's GPU context is still alive.
         *   nullptr before Startup, or with no device — which is also what a headless test gets.
         */
        virtual TUniquePtr<ITexture2D> CreateTexture(const void* InPixels, Uint32 InWidth,
                                                     Uint32 InHeight, Int32 InChannels) = 0;

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
        virtual EngineRegistries&   GetRegistries() = 0;

        virtual ResourceManager&            GetResources() = 0;
        virtual EngineEventBus&             GetEngineEventBus() = 0;
        virtual WorldManager&               GetWorldManager() = 0;

        /**
         * Enqueue from anywhere in the frame BEFORE the render that should show it — the renderer drains and clears it every frame, so a line must be
         * re-submitted each frame it stays visible. Serves editor overlays and dev builds of the game alike; the engine has no idea which one is calling.
         */
        virtual DebugDraw& GetDebugDraw() = 0;

        /**
         * The engine end of the input chain — the application FEEDS this as OS events arrive, and everything else reads it. 
         * Physical keys only: action maps are a game-layer concept built on top. 
         * Read-only for every caller except the application that owns the feed.
         */
        virtual InputManager& GetInput() = 0;

        // NOTE: frame stats are NOT here. They are an app service (IStatsService, I4): a passive
        // facility you submit scopes to, which the HOST tells about the frame boundary. The engine
        // is a consumer like anything else — it caches a FrameProfiler* and names four scopes.

        // End Foundation subsystems
        // =============================================================================
        
    public:
        //----- null object ----------------------------------------------------
        static IEngine& Null();
    };
}
