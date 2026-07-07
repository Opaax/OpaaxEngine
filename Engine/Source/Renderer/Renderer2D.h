#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Application/Services/ILogger.h"

#include <glm/glm.hpp>

#include "Assets/AssetHandle.hpp"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    class Texture2D;
    class ITexture2D;
    class ICamera;
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
     *          renderer.Begin(camera, cmd);
     *          renderer.DrawQuad({0,0}, {100,100}, {1,0,0,1});     // red quad
     *          renderer.DrawSprite({200,0}, {64,64}, myTexture);   // textured sprite
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
        // Out-of-line — the owned UniquePtr<Renderer2DData> holds a forward-declared type.
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
        // created via InDevice, so nothing routes through the global backend factory.
        void Init(IRHIDevice& InDevice, const RenderLimits& InLimits, const ShaderDesc& InShader);

        // Transitional: build via the global I*::Create factories, reading Sprite.glsl off disk.
        // Kept only for the dead old RenderSubsystem path; removed when that path is dismantled.
        void Init(const OpaaxString& InShaderSourcePath);

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

        // Transitional camera-object entry — kept for the dead old passes. Prefer BeginScene.
        void Begin(ICamera& InCamera, ICommandBuffer& InCmd);

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

        // Textured sprite — via AssetHandle (preferred, safe)
        void DrawSprite(const Vector2F&      InPosition,
                        const Vector2F&      InSize,
                        const TextureHandle& InTexture,
                        const Vector4F&      InColor        = Vector4F(1.f),
                        float                InRotationRad  = 0.f,
                        ERenderLayer         InLayer        = ERenderLayer::Default,
                        Int16                InOrderInLayer = 0);

        // Sprite sheet / atlas sub-region — via AssetHandle. UV in normalised [0,1] space.
        void DrawSprite(const Vector2F&      InPosition,
                        const Vector2F&      InSize,
                        const TextureHandle& InTexture,
                        const Vector2F&      InUVMin,
                        const Vector2F&      InUVMax,
                        const Vector4F&      InColor        = Vector4F(1.f),
                        float                InRotationRad  = 0.f,
                        ERenderLayer         InLayer        = ERenderLayer::Default,
                        Int16                InOrderInLayer = 0);

        // Draw a textured sprite, tinted by InColor (default white = no tint).
        void DrawSprite(const Vector2F& InPosition,
                        const Vector2F& InSize,
                        Texture2D&      InTexture,
                        const Vector4F& InColor        = Vector4F(1.f),
                        float           InRotationRad  = 0.f,
                        ERenderLayer    InLayer        = ERenderLayer::Default,
                        Int16           InOrderInLayer = 0);

        // Draw a textured sprite with UV sub-region (sprite sheet / atlas).
        void DrawSprite(const Vector2F& InPosition,
                        const Vector2F& InSize,
                        Texture2D&      InTexture,
                        const Vector2F& InUVMin,
                        const Vector2F& InUVMax,
                        const Vector4F& InColor        = Vector4F(1.f),
                        float           InRotationRad  = 0.f,
                        ERenderLayer    InLayer        = ERenderLayer::Default,
                        Int16           InOrderInLayer = 0);

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        void  BeginInternal(const Matrix44F& InViewProjection, ICommandBuffer& InCmd);
        void  Flush();
        void  StartBatch();
        float GetTextureSlot(ITexture2D& InTexture);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        UniquePtr<Renderer2DData> m_Data; // pImpl — GPU + batch state (defined in the .cpp)
    };

} // namespace Opaax
