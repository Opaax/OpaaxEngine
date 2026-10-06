#pragma once

#include "Application/Services/IAppService.h"
#include "Core/OpaaxTypes.h"
#include "World/WorldSpec.h"

namespace Opaax
{
    class EngineEventBus;
    class ResourceManager;
    class WorldManager;
    class InputManager;
    class GameInstanceManager;
    class World;
    class UICanvas;
    class IFramebuffer;
    class IRenderTarget;
    class ITexture2D;
    class DebugDraw;
    class EngineRegistries;
    struct FramebufferSpec;
    struct CameraView;

    // =============================================================================
    // IEngine — the engine as an application service. Owns the engine subsystems
    // and ticks them each frame. The application drives its lifecycle.
    // =============================================================================
    class IEngine : public IAppService
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
         * Starts the engine subsystems. Call once.
         */
        virtual bool Startup() = 0;

        /**
         * Starts a game: creates the GameInstance and its subsystems.
         * Must be called before the first world is created. Fails if a game is already running.
         * @return True if a game is now running
         */
        virtual bool StartGame() = 0;

        /**
         * Ends the game: destroys every Play world, then the GameInstance.
         * Does nothing if no game is running.
         * @return True if a game was ended
         */
        virtual bool EndGame() = 0;

        /**
         * Opens the startup world.
         * @param InSpec Which world to open, in which mode
         * @return The created world, already active. Null if the engine is not started.
         */
        virtual World* FinishStartup(const WorldSpec& InSpec) = 0;

        /**
         * Opens a level into a new world, activates it, then destroys the previous one.
         * An empty LevelPath gives an empty world.
         * @return The new world, already active. Null if the engine is not started.
         */
        virtual World* OpenLevel(const WorldSpec& InSpec) = 0;

        /**
         * Opens a level at the start of the next frame (for gameplay code).
         * Publishes LevelLoadRequested now and LevelLoadFinished after the swap,
         * so a loading screen can be shown. A newer request replaces a pending one.
         */
        virtual void RequestOpenLevel(const WorldSpec& InSpec) = 0;

        /**
         * Runs one engine frame.
         */
        virtual void Loop()                             = 0;

        /**
         * Swaps the window backbuffer. Called once per frame, after rendering.
         */
        virtual void PresentBackbuffer()                = 0;

        /**
         * Draws the active world into InTarget for this frame only. Submit again every frame.
         * With no submission, the world is drawn to the backbuffer.
         * @param InTarget Borrowed for the frame
         * @param InView Camera view, in world units
         * @param bInDrawOverlays Draw debug shapes in this view
         * @param bInDrawUI Draw the submitted UI canvases over this view
         */
        virtual void SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays,
                                      World* InSource = nullptr, bool bInDrawUI = false) = 0;

        /**
         * Draws InCanvas for this frame only. Submit again every frame.
         * @param InCanvas Borrowed for the frame
         * @param InTarget Null draws over every view with UI enabled; otherwise only into
         *   this target, cleared first (editor preview)
         * @param InView Optional view for InTarget (zoom/pan). Null uses the canvas's own view.
         */
        virtual void SubmitUICanvas(UICanvas& InCanvas, IRenderTarget* InTarget = nullptr,
                                    const CameraView* InView = nullptr) = 0;
        
        /**
         * Creates a framebuffer. Release it while the engine is still alive.
         * @return Null before Startup or without a device
         */
        virtual TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec) = 0;

        /**
         * Creates a GPU texture from decoded pixels.
         * @param InPixels Tightly packed, InWidth * InHeight * InChannels bytes. Copied.
         * @param InChannels 4 = RGBA8, 3 = RGB8, 1 = R8
         * @return Release it while the engine is still alive. Null before Startup or without a device.
         */
        virtual TUniquePtr<ITexture2D> CreateTexture(const void* InPixels, Uint32 InWidth,
                                                     Uint32 InHeight, Int32 InChannels) = 0;

        /**
         * Called once per frame, before physics and render.
         */
        virtual void Update(double InDeltaTime)         = 0;

        /**
         * Called zero or more times per frame, after Update and before Render.
         */
        virtual void FixedUpdate(double InFixedDeltaTime) = 0;

        /**
         * Called once per frame, after Update and physics.
         * @param InAlphaPhysicStep Interpolation factor between physics steps
         */
        virtual void Render(double InAlphaPhysicStep)   = 0;
        
        /**
         * First shutdown step: everything is still alive. Subsystems release here
         * anything that depends on another subsystem.
         */
        virtual void TearDown()                         = 0;

        /**
         * Second shutdown step: destroys the subsystems in reverse order. Safe to call twice.
         * Other subsystems may already be gone.
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
         * Owns the running GameInstance, if any. Check IsGameRunning first.
         */
        virtual GameInstanceManager&        GetGameInstances() = 0;

        /**
         * Debug shapes, cleared every frame. Submit before the render, every frame.
         */
        virtual DebugDraw& GetDebugDraw() = 0;

        /**
         * Keyboard and mouse state, fed by the application. Read-only for everyone else.
         */
        virtual InputManager& GetInput() = 0;

        // End Foundation subsystems
        // =============================================================================
        
    public:
        //----- null object ----------------------------------------------------
        static IEngine& Null();
    };
}
