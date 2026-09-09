#pragma once

#include "Core/EngineAPI.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // the texture cache holds Refs BY VALUE
#include "Renderer/CameraView.h"  // a submitted view holds one BY VALUE
#include "Renderer/DebugDraw.h"   // owned BY VALUE — full type, not a forward decl
#include "World/Entity/EntityTypes.h"   // EntityID — PoseFor takes one


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
    struct SpriteSheetResource;
    struct SpriteSheetData;
    struct SpriteComponent;
    struct SpriteUVRect;
    struct WindowResize;
    struct FontFaceResource;
    struct FontFamilyResource;
    struct FontFamilyData;
    struct FontStyleKey;
    struct FontFaceView;
    struct TextComponent;
    struct TransformComponent;
    struct DisplayPose;   // returned by value; only PoseFor's DEFINITION needs it complete

    // INCLUDED, not forward-declared (⑦-C **K5**). Its second parameter is defaulted, and a default
    // may be stated only once — on the definition — so a forward declaration here could no longer
    // let this header write `TResourcePath<TextureResource>`. Including it costs nothing the old
    // declaration was protecting against: ResourcePath.h pulls in a string and <type_traits>, and
    // names no part of the resource system. (See the include block above.)

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

        /**
         * ONE pass: compose the matrices for InTarget's pixels, open the pass, draw the world, then
         * the debug overlays if this view wants them.
         *
         * InView is in world units and the matrices are composed HERE, against this target's size —
         * that is CAM1's split, and it is what lets two views of different sizes frame the same
         * world correctly without either producer knowing about pixels.
         *
         * @param bInDrawOverlays False for a view that must look like the GAME — a camera preview
         *   shows no grid, no selection outline and no entity icons.
         */
        void RenderPass(IRenderTarget& InTarget, World* InWorld, const CameraView& InView, bool bInDrawOverlays);

        /**
         * Say ONCE that a frame needed more than one pass, naming the count.
         *
         * A smoke run cannot read the Stats panel, and "multi-view works" is not a thing a log can
         * say — a NUMBER is (L59). One shot, because the answer stops being news after the first.
         */
        void ReportPassCount(Uint32 InPasses);

        /** Every SpriteComponent in InWorld, in one pass. Split from RenderFrame so the world's
         *  two draw sources read as two lines, not as one long body. */
        void DrawWorldSprites(World& InWorld, Renderer2D& InRenderer);

        /** Every TextComponent in InWorld. DrawWorldSprites' twin, one draw source per body. */
        void DrawWorldTexts(World& InWorld, Renderer2D& InRenderer);

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

        /**
         * The sheet behind an asset-relative path, loading it once and keeping the claim.
         *
         * ResolveTexture's twin, cache and all — a sheet is a resource like any other and the same
         * "one lookup per draw, one load per session" rule applies. Null when the path is empty.
         */
        const SpriteSheetData* ResolveSheet(const TResourcePath<SpriteSheetResource>& InPath);

        /**
         * What one sprite draws: its texture, and the sub-rectangle of it to sample.
         *
         * The ONE place the Sheet-wins-over-Texture precedence lives, so the Inspector's tooltip and
         * the frame cannot disagree. A sheet naming a frame that does not exist warns ONCE and falls
         * back to the whole texture rather than drawing nothing, which would read as a broken sprite.
         *
         * @return false when there is nothing to draw at all — the ordinary "no image named yet".
         */
        bool ResolveSpriteDraw(const SpriteComponent& InSprite, ITexture2D*& OutTexture, SpriteUVRect& OutUV);

        /**
         * The face behind an asset-relative `.ttf` path, loading it once and keeping the claim.
         *
         * ResolveTexture's shape a third time. Answers the metrics AND the atlas together, because
         * the layout walker needs both and neither is usable alone.
         */
        FontFaceView ResolveFace(const TResourcePath<FontFaceResource>& InPath);

        /**
         * The family behind an asset-relative `.opaaxfont` path, loading it once and keeping the
         * claim. Null when the path is empty.
         */
        const FontFamilyData* ResolveFamily(const TResourcePath<FontFamilyResource>& InPath);

        /**
         * What one text component draws with.
         *
         * The ONE place the Font-wins-over-Face precedence lives, so the Inspector's tooltip and the
         * frame cannot disagree. A family that has nothing in the requested SCRIPT, or that answers
         * a different cut than the one asked for, warns ONCE — a family is consulted every frame, so
         * anything per-draw would be noise rather than a diagnostic.
         */
        FontFaceView ResolveTextDraw(const TextComponent& InText);

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
         * Draw the active world into InTarget, framed by InView, for THIS FRAME ONLY.
         *
         * IMMEDIATE MODE, exactly like the debug queue beside it (F4): the list is drained by
         * Render() and cleared every frame, so a producer that wants its view keeps submitting. That
         * is what makes a panel that hides — or dies — stop costing a pass with nothing to unregister,
         * and why there is no dangling-target window to order a shutdown around.
         *
         * SUBMITTING NOTHING IS THE RUNTIME PATH: a frame with no submissions draws the backbuffer
         * framed by the active world, which is what this always did.
         *
         * @param InTarget BORROWED for the frame — the submitter owns it (I5).
         * @param InView In WORLD units; the matrices are composed against InTarget's pixels (CAM1).
         * @param bInDrawOverlays Whether the debug queue draws in this view. False makes it look
         *   like the game.
         */
        void SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays);

        /**
         * Create an offscreen framebuffer on the render core's device (F2a). The natural companion to
         * SubmitRenderView: a caller that wants the world in a texture needs both — the backing
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
        void Render(double InAlpha) override;
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Types
        // =============================================================================
    private:
        /** One submitted view: where it lands, how the world is framed for it, and what it shows. */
        struct RenderPassRequest
        {
            IRenderTarget* Target        = nullptr;  // non-owning; the submitter owns it (I5)
            CameraView     View;
            bool           bDrawOverlays = true;
        };

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<RenderSystem> m_RenderSystem;
        WorldManager*           m_WorldManager  = nullptr; // non-owning; active world = draw source

        // This frame's views, cleared beside the debug queue in Render() (F4). Keeps its capacity,
        // so a steady frame allocates nothing.
        TDynArray<RenderPassRequest> m_SubmittedViews;

        /** Whether ReportPassCount has already spoken. One line per session, not one per frame. */
        bool m_bMultiPassLogged = false;

        // ④ — resolved in Startup like m_WorldManager. This subsystem opts IN to being measured;
        // nothing times it on its behalf.
        FrameProfiler*          m_Profiler      = nullptr;

        /**
         * Where InEntity should be DRAWN: its raw pose, or the blend toward it when a fixed step
         * wrote a previous one. Counts the blends it performs, so "interpolation is on" and
         * "something was actually interpolated" stay different claims ([[L15]]).
         */
        DisplayPose PoseFor(World& InWorld, EntityID InEntity, const TransformComponent& InTransform);

        /**
         * This frame's progress through the fixed step, for DISPLAY only (**PH21**). Zero when
         * interpolation is off, which makes every draw site read the raw pose with no branch.
         */
        float m_FrameAlpha = 0.f;

        /** Render.Interpolation, read once at Startup like every other config field. */
        bool m_bInterpolate = true;

        /** Blends performed on the last frame, and the one-shot that reports the first of them. */
        Uint64 m_BlendedThisFrame  = 0;
        bool   m_bLoggedFirstBlend = false;

        // Per-frame debug lines. Owned here because this is what DRAINS it (I5): the queue's
        // lifetime is the renderer's, and it cannot outlive its only consumer.
        DebugDraw               m_DebugDraw;

        // Interned asset path -> the claim keeping that texture loaded. Owned HERE because this is
        // the one render-side class allowed to reach the ResourceManager (Renderer2D stays
        // portable), and released in Shutdown — which runs BEFORE the ResourceManager's, i.e. while
        // the GL context is still alive to delete the GPU handles.
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_TextureCache;

        /** The same claim-and-keep cache for sheets. Released in Shutdown beside the textures'. */
        TUnorderedMap<Uint32, ResourceRef<SpriteSheetResource>> m_SheetCache;

        /** Sheets already warned about for naming a frame they do not have — one line, not one per frame. */
        TUnorderedSet<Uint32> m_WarnedFrameRange;

        /** The same claim-and-keep caches for text: one per `.ttf`, one per `.opaaxfont`. */
        TUnorderedMap<Uint32, ResourceRef<FontFaceResource>>   m_FaceCache;
        TUnorderedMap<Uint32, ResourceRef<FontFamilyResource>> m_FamilyCache;

        /**
         * (family, style) pairs already warned about for resolving to nothing or to a different cut.
         *
         * Keyed on the REQUEST rather than on the family, because a family that lacks Greek and is
         * asked for both Greek and Cyrillic has two things to say. Uint64 so the interned path id and
         * the four packed axes both fit without colliding.
         */
        TUnorderedSet<Uint64> m_WarnedFontStyle;
    };
}
