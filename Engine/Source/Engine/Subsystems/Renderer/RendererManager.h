#pragma once

#include "Core/EngineAPI.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"
#include "RHI/ICommandBuffer.h"    // ELoadOp
#include "Renderer/CameraView.h"
#include "Renderer/DebugDraw.h"
#include "UI/UIAssetProvider.h"
#include "World/Entity/EntityTypes.h"   // EntityID


// =============================================================================
// RendererManager
// =============================================================================
namespace Opaax
{
    class RenderSystem;
    class Renderer2D;
    class Config_Renderer;
    class Config_Engine;
    class World;
    class WorldManager;
    class IFramebuffer;
    class IRenderTarget;
    class ITexture2D;
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
    class UICanvas;
    struct DisplayPose;

    inline constexpr LogCategory LogRendererManager{"RendererManager"};

    // =============================================================================
    // RendererManager — engine adapter for the portable RenderSystem. Reads the engine
    //   config, services and paths, builds a RenderSystemDesc, owns the RenderSystem and
    //   drives its frame. The RenderSystem itself knows nothing about the engine.
    // =============================================================================
    class OPAAX_API RendererManager final : public EngineSubsystemBase, public IUIAssetProvider
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
        // Out-of-line: RenderSystem is forward-declared.
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
         * Window resize (event bus): resizes the backbuffer.
         */
        void HandleWindowResize(const WindowResize& InResize);

        /**
         * Renderer.config changed: applies ClearColor. Batch limits need a restart.
         */
        void HandleRendererConfigChanged();

        /** Engine.config changed: applies Render.bInterpolation. Backend needs a restart. */
        void HandleEngineConfigChanged();

        /**
         * The actual rendering. Separate from Render() so the per-frame queues are always cleared.
         */
        void RenderFrame();

        /**
         * One pass: builds the matrices for InTarget's size, draws the world, then the debug overlays.
         * @param bInDrawOverlays False for a game-looking view (no grid, outline or icons)
         */
        void RenderPass(IRenderTarget& InTarget, World* InWorld, const CameraView& InView, bool bInDrawOverlays);

        /**
         * Draws every submitted canvas over a world view, laid out for this target's size
         * (second pass, keeps what the world pass drew).
         */
        void RenderCanvases(IRenderTarget& InTarget);

        /**
         * Draws one canvas into InTarget. With InView, the submitter already laid it out
         * (editor zoom); otherwise the target size drives the layout.
         */
        void RenderCanvasPass(UICanvas& InCanvas, IRenderTarget& InTarget, ELoadOp InLoadOp, const CameraView* InView = nullptr);

        /**
         * Logs once the first time a frame needs more than one pass.
         */
        void ReportPassCount(Uint32 InPasses);

        /** Draws every SpriteComponent in InWorld. */
        void DrawWorldSprites(World& InWorld, Renderer2D& InRenderer);

        /** Draws every TextComponent in InWorld. */
        void DrawWorldTexts(World& InWorld, Renderer2D& InRenderer);

        /**
         * Publishes the batch counters as named stats. Called even when nothing was drawn.
         */
        void SubmitRenderCounters();

        /**
         * The GPU texture for an asset-relative path. Loaded once and cached.
         * Null for an empty path or while uploading; a failed load gives the magenta placeholder.
         */
        ITexture2D* ResolveTexture(const TResourcePath<TextureResource>& InPath);

        /**
         * The sprite sheet for an asset-relative path. Loaded once and cached. Null for an empty path.
         */
        const SpriteSheetData* ResolveSheet(const TResourcePath<SpriteSheetResource>& InPath);

        /**
         * What a sprite draws: its texture and the region to sample. A sheet wins over a texture.
         * A missing sheet frame warns once and uses the whole texture.
         * @return False when there is nothing to draw
         */
        bool ResolveSpriteDraw(const SpriteComponent& InSprite, ITexture2D*& OutTexture, SpriteUVRect& OutUV);

        /**
         * The font face (metrics and atlas) for an asset-relative .ttf path. Loaded once and cached.
         */
        FontFaceView ResolveFace(const TResourcePath<FontFaceResource>& InPath);

        /** IUIAssetProvider — the same caches, for UI widgets. */
        FontFaceView     ResolveFace(const char* InAssetPath) override;
        ITexture2D*      ResolveTexture(const char* InAssetPath) override;
        UISheetFrameView ResolveSheetFrame(const char* InSheetPath, Int32 InFrame) override;

        /**
         * The font family for an asset-relative .opaaxfont path. Loaded once and cached.
         * Null for an empty path.
         */
        const FontFamilyData* ResolveFamily(const TResourcePath<FontFamilyResource>& InPath);

        /**
         * What a text component draws with. A family wins over a face.
         * Warns once when a family has nothing for the script, or gives a different style.
         */
        FontFaceView ResolveTextDraw(const TextComponent& InText);

        // =============================================================================
        // Getters - Setter
    public:
        /**
         * Swaps the backbuffer. Called by Engine::PresentBackbuffer, after the editor UI.
         * Does nothing if the render core failed to start.
         */
        void Present();

        /**
         * Draws the active world into InTarget for this frame only. Submit again every frame.
         * With no submission, the world is drawn to the backbuffer.
         * @param InTarget Borrowed for the frame
         * @param InView Camera view, in world units
         * @param bInDrawOverlays Draw debug shapes in this view
         * @param bInDrawUI Draw the submitted UI canvases over this view
         */
        void SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays,
                              World* InSource = nullptr, bool bInDrawUI = false);

        /**
         * Draws InCanvas for this frame only. Submit again every frame.
         * @param InCanvas Borrowed for the frame
         * @param InTarget Null draws over every view with UI enabled; otherwise only into this
         *   target, cleared first (editor preview)
         * @param InView Optional view for InTarget (zoom/pan). Null uses the canvas's own view.
         */
        void SubmitUICanvas(UICanvas& InCanvas, IRenderTarget* InTarget = nullptr, const CameraView* InView = nullptr);

        /**
         * Creates an offscreen framebuffer. Release it before the render core shuts down.
         * @return Null before Startup or without a device
         */
        TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec);

        /**
         * Uploads decoded pixels to a GPU texture (used by TextureResource::Initialize).
         * Release it before the render core shuts down.
         * @return Null before Startup
         */
        TUniquePtr<ITexture2D> CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        /**
         * @return The debug shape queue, cleared every frame by Render()
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
        /** One submitted view: target, camera view and what it shows. */
        struct RenderPassRequest
        {
            IRenderTarget* Target        = nullptr;  // not owned
            CameraView     View;
            bool           bDrawOverlays = true;
            bool           bDrawUI       = false;

            /**
             * The world this view draws. Null means the active world.
             */
            World*         Source        = nullptr;  // not owned
        };

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<RenderSystem> m_RenderSystem;

        /** Subscribed in Startup, released in Shutdown. Not owned. */
        Config_Renderer* m_RendererConfig = nullptr;
        Config_Engine*   m_EngineConfig   = nullptr;
        WorldManager*           m_WorldManager  = nullptr; // not owned

        // This frame's views, cleared every frame. Keeps its capacity.
        TDynArray<RenderPassRequest> m_SubmittedViews;

        /** One submitted canvas, and optionally its own target. */
        struct UICanvasRequest
        {
            UICanvas*      Canvas = nullptr;   // not owned
            IRenderTarget* Target = nullptr;   // null = over the world views
            CameraView     View;               // used only with bHasView
            bool           bHasView = false;
        };

        /** This frame's canvases. */
        TDynArray<UICanvasRequest> m_SubmittedCanvases;

        /** This frame's UI counters. */
        Uint32 m_UILayouts  = 0;
        Uint32 m_UIRebuilds = 0;
        bool   m_bLoggedFirstUI = false;

        /** Whether ReportPassCount already logged. */
        bool m_bMultiPassLogged = false;

        /**
         * Where InEntity is drawn: its pose, or interpolated from the previous fixed step.
         * Counts the blends it performs.
         */
        DisplayPose PoseFor(World& InWorld, EntityID InEntity, const TransformComponent& InTransform);

        /**
         * Progress through the fixed step, for display only. Zero when interpolation is off.
         */
        float m_FrameAlpha = 0.f;

        /** Render.bInterpolation — read at Startup, updated on Engine.config change. */
        bool m_bInterpolate = true;

        /** Blends done last frame, and whether the first one was logged. */
        Uint64 m_BlendedThisFrame  = 0;
        bool   m_bLoggedFirstBlend = false;

        // Debug shapes. Owned here because the renderer consumes them.
        DebugDraw               m_DebugDraw;

        // Asset path -> loaded texture. Released in Shutdown, before the ResourceManager's,
        // while the GL context is still alive.
        TUnorderedMap<Uint32, ResourceRef<TextureResource>> m_TextureCache;

        /** Same cache for sprite sheets. */
        TUnorderedMap<Uint32, ResourceRef<SpriteSheetResource>> m_SheetCache;

        /** Sheets already warned about for a missing frame. */
        TUnorderedSet<Uint32> m_WarnedFrameRange;

        /** Same caches for fonts: .ttf faces and .opaaxfont families. */
        TUnorderedMap<Uint32, ResourceRef<FontFaceResource>>   m_FaceCache;
        TUnorderedMap<Uint32, ResourceRef<FontFamilyResource>> m_FamilyCache;

        /**
         * (family, style) pairs already warned about. Font path id and packed style in one Uint64.
         */
        TUnorderedSet<Uint64> m_WarnedFontStyle;
    };
}
