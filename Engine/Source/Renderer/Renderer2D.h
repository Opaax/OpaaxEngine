#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"

#include "Renderer/RenderLayer.h"

namespace Opaax
{
    class ITexture2D;
    class ICommandBuffer;
    class IRHIDevice;
    struct Renderer2DData;
    struct RenderView;
    struct RenderLimits;
    struct ShaderDesc;

    inline constexpr LogCategory LogRenderer2D{"Renderer2D"};

    /**
     * @class Renderer2D
     *
     * Instance-owned 2D batch renderer (one per render department — RendererManager owns it).
     * Stateless from the caller's perspective per frame: call Begin/End around your draw calls.
     * Internally accumulates a vertex batch and flushes when full or when all texture slots are
     * occupied. One draw call per flush. All GPU state lives in the pImpl (Renderer2DData).
     *
     * Usage:
     *          renderer.BeginScene(view, cmd);
     *          renderer.DrawQuad({0,0}, {100,100}, {1,0,0,1});     // red quad
     *          renderer.DrawSprite({0,0}, {100,100}, texture);     // textured quad
     *          renderer.End();
     *
     * Init()/Shutdown() build/release the GPU resources — call from the owner's Startup/Shutdown
     * (render API up, context current), never mid-frame.
     */
    class OPAAX_API Renderer2D
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        // Out-of-line — the owned TUniquePtr<Renderer2DData> holds a forward-declared type.
        Renderer2D();
        ~Renderer2D();

        // =============================================================================
        // Copy - Move Delete (owns GPU handles)
        // =============================================================================
        Renderer2D(const Renderer2D&)            = delete;
        Renderer2D& operator=(const Renderer2D&) = delete;
        Renderer2D(Renderer2D&&)                 = delete;
        Renderer2D& operator=(Renderer2D&&)      = delete;

        // =============================================================================
        // Lifecycle
        // =============================================================================
    public:
        // Build the batch GPU resources through the device (the live path). All resources are
        // created via InDevice, so nothing routes through a global backend factory.
        void Init(IRHIDevice& InDevice, const RenderLimits& InLimits, const ShaderDesc& InShader);

        void Shutdown();

        // =============================================================================
        // Begin / End
        // =============================================================================
    public:
        /**
         * Open a scene from a per-frame RenderView snapshot (the live path — no camera object).
         * Sets the viewport, uploads the view-projection, and starts a fresh batch. Draws issued
         * until End() record into InCmd.
         */
        void BeginScene(const RenderView& InView, ICommandBuffer& InCmd);

        // Call once per frame after all draw calls — flushes the remaining batch.
        void End();

        // =============================================================================
        // Draw calls
        // =============================================================================
    public:
        /**
         * Draw a solid-colour quad.
         * @param InPosition centre of the quad (Y-up world space)
         * @param InSize full width and height
         * @param InColor RGBA normalised [0,1]
         * @param InRotationRad rotation around the quad centre, radians, CCW (default 0 = axis-aligned fast path)
         * @param InLayer coarse draw-order band (default Default)
         * @param InOrderInLayer fine tie-break within the band, lower = behind (default 0)
         */
        void DrawQuad(const Vector2F& InPosition,
                      const Vector2F& InSize,
                      const Vector4F& InColor,
                      float           InRotationRad  = 0.f,
                      ERenderLayer    InLayer        = ERenderLayer::Default,
                      Int16           InOrderInLayer = 0);

        /**
         * Draw a textured quad. The texture is bound to one of the batch's sampler slots; a batch
         * that runs out of slots flushes and starts a new one, so a caller never manages binding.
         *
         * @param InPosition centre of the quad (Y-up world space)
         * @param InSize full width and height
         * @param InTexture sampled texture. BORROWED for the batch — it must outlive the flush,
         *   which the owning ResourceRef guarantees for the frame it draws in.
         * @param InTint multiplied into the sample; white draws the texture unchanged
         * @param InRotationRad rotation around the quad centre, radians, CCW
         * @param InLayer coarse draw-order band
         * @param InOrderInLayer fine tie-break within the band, lower = behind
         * @param InUVMin / @param InUVMax sub-rectangle to sample. Defaulted to the whole texture,
         *   and present so an atlas — a sprite sheet, a glyph — needs no second entry point.
         */
        void DrawSprite(const Vector2F& InPosition,
                        const Vector2F& InSize,
                        ITexture2D&     InTexture,
                        const Vector4F& InTint         = { 1.f, 1.f, 1.f, 1.f },
                        float           InRotationRad  = 0.f,
                        ERenderLayer    InLayer        = ERenderLayer::Default,
                        Int16           InOrderInLayer = 0,
                        const Vector2F& InUVMin        = { 0.f, 0.f },
                        const Vector2F& InUVMax        = { 1.f, 1.f });

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        void  BeginInternal(const Matrix44F& InViewProjection, ICommandBuffer& InCmd);
        void  Flush();
        void  StartBatch();

        /** Flush + restart the batch if the vertex buffer is full. Called before anything that
         *  derives state FROM the batch (a texture slot), because a flush resets it. */
        void  EnsureBatchRoom();

        /**
         * The ONE body that writes a quad's four vertices and its sort key. A coloured quad is a
         * sprite on slot 0 sampling the whole white texture, so both entry points come here — one
         * place for the winding, the rotation and the key to be right or wrong.
         */
        void  SubmitQuad(const Vector2F& InPosition,
                         const Vector2F& InSize,
                         const Vector4F& InColor,
                         float           InRotationRad,
                         ERenderLayer    InLayer,
                         Int16           InOrderInLayer,
                         float           InTexIndex,
                         const Vector2F& InUVMin,
                         const Vector2F& InUVMax);

        /**
         * The slot InTexture is bound to for this batch: an existing one if it is already bound,
         * else a fresh one — flushing first when all slots are taken.
         */
        float GetTextureSlot(ITexture2D& InTexture);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<Renderer2DData> m_Data; // pImpl — GPU + batch state (defined in the .cpp)
    };

} // namespace Opaax
