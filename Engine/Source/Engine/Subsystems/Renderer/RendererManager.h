#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // the texture cache holds Refs BY VALUE
#include "Renderer/DebugDraw.h"   // owned BY VALUE — full type, not a forward decl


// =============================================================================
// RendererManager
// =============================================================================
namespace Opaax
{
    class RenderSystem;
    class Renderer2D;
    class World;
    class WorldManager;
    class IFramebuffer;
    class IRenderTarget;
    class ITexture2D;
    class FrameProfiler;
    struct FramebufferSpec;
    struct TextureResource;
    struct WindowResize;

    template<typename TResource>
    struct TResourcePath;

    inline constexpr LogCategory LogRendererManager{"RendererManager"};

    // =============================================================================
    // RendererManager — the ENGINE ADAPTER for the portable RenderSystem. This is the
    //   only render-side code allowed to reach host globals (services/config/paths): it
    //   resolves them, builds a RenderSystemDesc, owns one RenderSystem, and drives its
    //   frame each tick. All actual rendering lives in the RenderSystem module, which knows
    //   nothing of this engine — so the same core runs unchanged in any other host.
    // =============================================================================
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
        // Out-of-line — the owned TUniquePtr<RenderSystem> holds a forward-declared type.
        RendererManager();
        ~RendererManager() override;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        RendererManager(const RendererManager&)            = delete;
        RendererManager& operator=(const RendererManager&) = delete;
        RendererManager(RendererManager&&)                 = delete;
        RendererManager& operator=(RendererManager&&)      = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * Bus handler
         * forwards a window resize to the render core (which resizes the backbuffer).
         * @param InResize The Event
         */
        void HandleWindowResize(const WindowResize& InResize);

        /**
         * The frame's actual rendering. Separated from Render() so the debug-queue drain there is
         * unconditional — this body early-outs (no render core, zero-size target) and those exits
         * must not leave the queue to accumulate.
         */
        void RenderFrame();

        /** Every SpriteComponent in InWorld, in one pass. Split from RenderFrame so the world's
         *  two draw sources read as two lines, not as one long body. */
        void DrawWorldSprites(World& InWorld, Renderer2D& InRenderer);

        /**
         * Publish the batcher's frame counters as named stats (④). Called from Render, OUTSIDE
         * RenderFrame's early-outs, so a frame that drew nothing reports zeros.
         */
        void SubmitRenderCounters();

        /**
         * The GPU texture behind an asset-relative path, loading it once and keeping the claim.
         *
         * Cached by INTERNED PATH, so a hundred sprites sharing one image resolve to one integer
         * lookup per draw and one load per session. Null when the path is empty (nothing to draw)
         * or the texture has not finished uploading; a path that fails to load answers the magenta
         * placeholder instead, which is visible rather than absent.
         */
        ITexture2D* ResolveTexture(const TResourcePath<TextureResource>& InPath);

        // =============================================================================
        // Getters - Setter
    public:
        /**
         * Present the backbuffer — called by Engine::PresentBackbuffer (host-driven, after TickFrame).
         * Separate from Render so the editor can draw UI to the backbuffer before the swap (S7). No-op
         * if the render core failed to start.
         */
        void Present();

        /**
         * Redirect the world render into InTarget instead of the backbuffer; nullptr restores the
         * backbuffer. Non-owning — the caller (editor's ViewportPanel) owns the target. Stored, then
         * read by Render() each frame to pick the target and its size (D2: the target's size drives
         * the view, replacing the old window-size cache).
         */
        void SetPrimaryRenderTarget(IRenderTarget* InTarget);

        /**
         * Create an offscreen framebuffer on the render core's device (F2a). The natural companion to
         * SetPrimaryRenderTarget: a caller that wants the world in a texture needs both — the backing
         * store, then the target wrapping it. CALLER-OWNED, and it must be released before the render
         * core shuts down. nullptr before Startup (no core yet) or if the device is gone.
         */
        TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec);

        /**
         * Upload decoded pixels to a GPU texture on the render core's device (F2a). Reached through
         * IEngine by TextureResource::Initialize — the same arrangement CreateFramebuffer has with
         * the editor's ViewportPanel, and for the same reason: the caller needs a GPU resource and
         * must not hold a device.
         *
         * CALLER-OWNED; release it before the render core shuts down. nullptr before Startup.
         */
        TUniquePtr<ITexture2D> CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        /**
         * @return The per-frame debug line queue, drained and cleared by Render(). Reached by game
         *   and editor code through IEngine::GetDebugDraw(); always valid (owned by value).
         */
        DebugDraw& GetDebugDraw() noexcept { return m_DebugDraw; }

        // End Getters - Setter
        // =============================================================================

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()             override;
        void Shutdown()            override;
        void Render(double Alpha)  override;
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<RenderSystem> m_RenderSystem;
        WorldManager*           m_WorldManager  = nullptr; // non-owning; active world = draw source
        IRenderTarget*          m_PrimaryTarget = nullptr; // non-owning; nullptr = backbuffer (I5)

        // ④ — resolved in Startup like m_WorldManager. This subsystem opts IN to being measured;
        // nothing times it on its behalf.
        FrameProfiler*          m_Profiler      = nullptr;

        // Per-frame debug lines. Owned here because this is what DRAINS it (I5): the queue's
        // lifetime is the renderer's, and it cannot outlive its only consumer.
        DebugDraw               m_DebugDraw;

        // Interned asset path -> the claim keeping that texture loaded. Owned HERE because this is
        // the one render-side class allowed to reach the ResourceManager (Renderer2D stays
        // portable), and released in Shutdown — which runs BEFORE the ResourceManager's, i.e. while
        // the GL context is still alive to delete the GPU handles.
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_TextureCache;
    };
}
