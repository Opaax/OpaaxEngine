#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"

#include "Core/Maths/Bounds2D.h"
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
     * Half-extent of an outlined quad's hole, in local 0..1 space. {0,0} is a solid quad.
     * Bad input (zero size, negative or too thick border) gives solid.
     */
    OPAAX_API Vector2F MakeOutlineInnerHalf(const Vector2F& InSize, float InThickness) noexcept;

    /**
     * Masks a quad: a rect in the quad's space and a texture sampled across it.
     * White shows, black hides (alpha *= mask.r * mask.a). A default QuadMask masks nothing.
     * A null Texture with a real Rect clips to the rect.
     */
    struct QuadMask
    {
        Bounds2D    Rect;
        ITexture2D* Texture = nullptr;   // borrowed for the batch

        /** A zero-size rect means no mask. */
        bool IsActive() const noexcept { return Rect.HalfExtent.x > 0.f && Rect.HalfExtent.y > 0.f; }
    };

    /**
     * One frame's batching counters (all passes). DrawCalls above 1 means the frame was split:
     * Quads past the limit = buffer full; PeakTextureSlots at the limit = samplers full.
     */
    struct Renderer2DStats
    {
        Uint32 Quads            = 0;
        Uint32 DrawCalls        = 0;
        Uint32 PeakTextureSlots = 0;
    };

    /**
     * 2D batch renderer (owned by RenderSystem). A pass is recorded whole, sorted, then split into
     * batches at EndPass, so draw order never depends on batching. One draw call per batch.
     *
     * Usage:
     *          renderer.BeginPass(view, cmd);
     *          renderer.DrawQuad({0,0}, {100,100}, {1,0,0,1});     // red quad
     *          renderer.DrawSprite({0,0}, {100,100}, texture);     // textured quad
     *          renderer.EndPass();
     *
     * Init()/Shutdown() create/release the GPU resources; never call them mid-frame.
     */
    class OPAAX_API Renderer2D
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        // Out-of-line: Renderer2DData is forward-declared.
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
         * Creates the batch GPU resources through InDevice.
         * InLimits sizes the batch (MaxQuads, MaxTextureSlots), clamped to what the shader and buffers
         * support (with a warning).
         */
        void Init(IRHIDevice& InDevice, const RenderLimits& InLimits, const ShaderDesc& InShader);

        void Shutdown();

        // =============================================================================
        // Begin / End
        // =============================================================================
    public:
        /**
         * Opens a pass: sets the viewport, uploads the view-projection, starts recording into InCmd.
         */
        void BeginPass(const RenderView& InView, ICommandBuffer& InCmd);

        // Closes the pass: sorts what was recorded, splits it into batches, draws them.
        void EndPass();

        // =============================================================================
        // Stats
        // =============================================================================
    public:
        /** This frame's batching counters (complete after the last pass). */
        const Renderer2DStats& GetStats() const noexcept;

        /** Resets the counters. Called by RenderSystem::BeginFrame. */
        void ResetStats() noexcept;

        // =============================================================================
        // Draw calls
        // =============================================================================
    public:
        /**
         * Draws a solid-colour quad.
         * @param InPosition Centre (Y-up world space)
         * @param InSize Full width and height
         * @param InColor RGBA [0,1]
         * @param InRotationRad Rotation around the centre, radians, counter-clockwise
         * @param InLayer Draw layer
         * @param InOrderInLayer Order within the layer, lower = behind
         */
        void DrawQuad(const Vector2F& InPosition,
                      const Vector2F& InSize,
                      const Vector4F& InColor,
                      float           InRotationRad  = 0.f,
                      ERenderLayer    InLayer        = ERenderLayer::Default,
                      Int16           InOrderInLayer = 0,
                      const QuadMask& InMask         = {});

        /**
         * Draws a textured quad.
         * @param InPosition Centre (Y-up world space)
         * @param InSize Full width and height
         * @param InTexture Borrowed until the batch is drawn
         * @param InTint Multiplied with the texture; white keeps it unchanged
         * @param InRotationRad Rotation around the centre, radians, counter-clockwise
         * @param InLayer Draw layer
         * @param InOrderInLayer Order within the layer, lower = behind
         * @param InUVMin / @param InUVMax Region to sample (default: whole texture)
         */
        void DrawSprite(const Vector2F& InPosition,
                        const Vector2F& InSize,
                        ITexture2D&     InTexture,
                        const Vector4F& InTint         = { 1.f, 1.f, 1.f, 1.f },
                        float           InRotationRad  = 0.f,
                        ERenderLayer    InLayer        = ERenderLayer::Default,
                        Int16           InOrderInLayer = 0,
                        const Vector2F& InUVMin        = { 0.f, 0.f },
                        const Vector2F& InUVMax        = { 1.f, 1.f },
                        const QuadMask& InMask         = {});

        /**
         * Draws a hollow quad: a border of InThickness, empty inside. Untextured.
         * @param InPosition Centre (Y-up world space)
         * @param InSize Full width and height, border included
         * @param InColor RGBA [0,1]
         * @param InThickness Border width in world units (thick enough fills it)
         * @param InRotationRad Rotation around the centre, radians, counter-clockwise
         * @param InLayer Draw layer
         * @param InOrderInLayer Order within the layer, lower = behind
         */
        void DrawQuadOutline(const Vector2F& InPosition,
                             const Vector2F& InSize,
                             const Vector4F& InColor,
                             float           InThickness,
                             float           InRotationRad  = 0.f,
                             ERenderLayer    InLayer        = ERenderLayer::Debug,
                             Int16           InOrderInLayer = 0,
                             const QuadMask& InMask         = {});

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** Clears the recording and sets the white texture as id 0. */
        void   StartPass();

        /** Plans the recorded quads, then draws them one batch at a time. */
        void   EmitPass();

        /** Uploads a batch, binds its samplers, issues one DrawIndexed. */
        void   Flush(Uint32 InQuadCount, Uint32 InSlotCount);

        /**
         * Writes a quad's four vertices and sort key. Used by every draw call.
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
                          const Vector2F& InInnerHalf = { 0.f, 0.f },
                          const QuadMask& InMask      = {});

        /**
         * InTexture's id for this pass (existing or new).
         */
        Uint32 GetTextureId(ITexture2D& InTexture);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<Renderer2DData> m_Data; // GPU and batch state (defined in the .cpp)
    };

} // namespace Opaax
