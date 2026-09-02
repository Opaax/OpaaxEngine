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
     * Half-extent of an outlined quad's HOLE, in local 0..1 space — what the shader compares the
     * fragment's local position against. `{0,0}` is a SOLID quad.
     *
     * Free and pure so the maths is testable with no GL context. Every degenerate input (zero size,
     * negative thickness, a border thick enough to swallow the box) answers solid rather than a
     * divide-by-zero or an inside-out hole.
     */
    OPAAX_API Vector2F MakeOutlineInnerHalf(const Vector2F& InSize, float InThickness) noexcept;

    /**
     * What one FRAME of batching cost. Per frame, not per pass — multi-view runs several passes
     * into one frame and the interesting numbers are the totals.
     *
     * DrawCalls is also the FLUSH count: Flush issues exactly one DrawIndexed, so shipping both
     * would be the same number twice. Above 1 the frame split, which is a COST and no longer a
     * correctness problem — a pass is sorted whole before it is cut. Why it split is readable from
     * the other two: Quads past the batch limit means the buffer filled, PeakTextureSlots at the
     * limit means the samplers did.
     */
    struct Renderer2DStats
    {
        Uint32 Quads            = 0;
        Uint32 DrawCalls        = 0;
        Uint32 PeakTextureSlots = 0;
    };

    /**
     * @class Renderer2D
     *
     * Instance-owned 2D batch renderer (one per render department — RendererManager owns it).
     * Stateless from the caller's perspective per frame: call BeginPass/EndPass around your draw
     * calls. A pass is RECORDED whole, then sorted once and cut into batches at EndPass — so draw
     * order never depends on where a flush landed. One draw call per batch. All GPU state lives in
     * the pImpl (Renderer2DData).
     *
     * Usage:
     *          renderer.BeginPass(view, cmd);
     *          renderer.DrawQuad({0,0}, {100,100}, {1,0,0,1});     // red quad
     *          renderer.DrawSprite({0,0}, {100,100}, texture);     // textured quad
     *          renderer.EndPass();
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
        /**
         * Build the batch GPU resources through the device (the live path). All resources are
         * created via InDevice, so nothing routes through a global backend factory.
         *
         * InLimits sizes the batch: the vertex/index buffers hold MaxQuads, and MaxTextureSlots
         * caps the samplers a single draw may bind. Both are clamped to what the sprite shader
         * and the buffers can honour, loudly.
         */
        void Init(IRHIDevice& InDevice, const RenderLimits& InLimits, const ShaderDesc& InShader);

        void Shutdown();

        // =============================================================================
        // Begin / End
        // =============================================================================
    public:
        /**
         * Open a pass from a per-frame RenderView snapshot (the live path — no camera object).
         * Sets the viewport, uploads the view-projection, and starts a fresh recording. Draws
         * issued until EndPass() record into InCmd.
         */
        void BeginPass(const RenderView& InView, ICommandBuffer& InCmd);

        // Close the pass: sort everything recorded, cut it into batches, draw them.
        void EndPass();

        // =============================================================================
        // Stats
        // =============================================================================
    public:
        /** This frame's batching counters, complete once the last pass has ended. */
        const Renderer2DStats& GetStats() const noexcept;

        /** Zero them. The FRAME owner calls this (RenderSystem::BeginFrame), never a pass. */
        void ResetStats() noexcept;

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

        /**
         * Draw a HOLLOW quad — a border of InThickness with nothing inside. ONE quad, not four
         * lines, so an editor overlay costs a quarter of what DrawBox used to.
         *
         * UNTEXTURED by design: the shader reads the fragment's texcoord as its LOCAL position to
         * find the border, which only holds while the UVs span the full 0..1. A textured outline
         * sampling an atlas sub-rect would carve the hole in the wrong place, so there is
         * deliberately no overload taking a texture.
         *
         * @param InPosition centre of the quad (Y-up world space)
         * @param InSize full width and height, border included
         * @param InColor RGBA normalised [0,1]
         * @param InThickness border width in world units. Thick enough to close the hole draws solid.
         * @param InRotationRad rotation around the centre, radians, CCW
         * @param InLayer coarse draw-order band
         * @param InOrderInLayer fine tie-break within the band, lower = behind
         */
        void DrawQuadOutline(const Vector2F& InPosition,
                             const Vector2F& InSize,
                             const Vector4F& InColor,
                             float           InThickness,
                             float           InRotationRad  = 0.f,
                             ERenderLayer    InLayer        = ERenderLayer::Debug,
                             Int16           InOrderInLayer = 0);

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** Drop the recording and re-seat the white texture as id 0. */
        void   StartPass();

        /** Plan the recorded quads, then walk the plan gathering and drawing one batch at a time. */
        void   EmitPass();

        /** Upload InQuadCount gathered quads, bind the batch's samplers, issue the one DrawIndexed. */
        void   Flush(Uint32 InQuadCount, Uint32 InSlotCount);

        /**
         * The ONE body that writes a quad's four vertices and its sort key. A coloured quad is a
         * sprite on texture id 0 sampling the whole white texture, so both entry points come here —
         * one place for the winding, the rotation and the key to be right or wrong.
         */
        void   SubmitQuad(const Vector2F& InPosition,
                          const Vector2F& InSize,
                          const Vector4F& InColor,
                          float           InRotationRad,
                          ERenderLayer    InLayer,
                          Int16           InOrderInLayer,
                          Uint32          InTexId,
                          const Vector2F& InUVMin,
                          const Vector2F& InUVMax,
                          const Vector2F& InInnerHalf = { 0.f, 0.f });

        /**
         * InTexture's id for this PASS — an existing one if it has been drawn already, else a fresh
         * one. A pass may name more textures than a batch can bind; which of them share a draw call
         * is the batch plan's answer, not this one's.
         */
        Uint32 GetTextureId(ITexture2D& InTexture);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<Renderer2DData> m_Data; // pImpl — GPU + batch state (defined in the .cpp)
    };

} // namespace Opaax
