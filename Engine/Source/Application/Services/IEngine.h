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
    class GameInstanceManager;
    class World;
    class IFramebuffer;
    class IRenderTarget;
    class ITexture2D;
    class DebugDraw;
    class EngineRegistries;
    struct FramebufferSpec;
    struct CameraView;

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
         * Begin a GAME: create the GameInstance and start every registered session subsystem.
         *
         * BEFORE THE FIRST WORLD, and that ordering is the contract. A world subsystem's context
         * is built inside CreateWorld, so a session created in reaction to a world would arrive
         * too late for every world that already exists.
         *
         * Called by a runtime host between OnModulesRegistered and FinishStartup, and by the
         * editor's PlayInEditor::Play. An editor sitting in an Edit world never calls it — there
         * is legitimately no game, and IsGameRunning answers false all session.
         *
         * REFUSES LOUDLY when a game is already running.
         *
         * @return true when a game is running as a result of this call.
         */
        virtual bool StartGame() = 0;

        /**
         * End the game: destroy every PLAY world, then the GameInstance. That order, and it is
         * the mirror of StartGame's.
         *
         * A SILENT no-op when no game is running — hosts call it unconditionally on the teardown
         * path, so "there was nothing to end" is a normal answer.
         *
         * Destroying "every Play world" is what makes this the editor's Stop: the edit world is
         * an Edit world, so the only thing that goes is the PIE clone.
         *
         * @return true when a game was actually ended.
         */
        virtual bool EndGame() = 0;

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
         * Draw the active world into InTarget, framed by InView, for THIS FRAME ONLY.
         *
         * IMMEDIATE MODE like the debug queue (F4): re-submit every frame, and stop submitting to
         * stop drawing. Nothing is registered, so nothing has to be cleared before a target dies —
         * the editor's ViewportPanel submits its offscreen FBO each frame so the world lands in a
         * texture, and a second panel submitting a second one is what multi-view is.
         *
         * A frame with NO submissions draws the backbuffer framed by the active world, which is the
         * runtime path and needs no caller at all.
         *
         * @param InTarget BORROWED for the frame — the submitter owns its lifetime (I5).
         * @param InView In WORLD units; the matrices are composed against InTarget's pixels (CAM1).
         * @param bInDrawOverlays Whether the debug queue draws in this view. False looks like the game.
         */
        virtual void SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays,
                                      World* InSource = nullptr) = 0;
        
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
         * Owns the running GameInstance (0 or 1) — ask it IsGameRunning before reaching further.
         * No game is a normal state, not a failure: it is what an editor in an Edit world has.
         */
        virtual GameInstanceManager&        GetGameInstances() = 0;

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
