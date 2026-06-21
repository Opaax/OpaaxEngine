#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"

#include <glm/glm.hpp>

#include "Assets/AssetHandle.hpp"
#include "Renderer/RenderLayer.h"
#include "Renderer/RenderStats.h"

namespace Opaax
{
    class Texture2D;
    class ICamera;
    class ICommandBuffer;

    /**
     * @class Renderer2D
     *
     * Stateless from the caller's perspective. Call Begin/End around your draw calls.
     * Internally accumulates a vertex batch and flushes when full or when all texture slots are occupied.
     *
     * One draw call per flush. Max batch size: MAX_QUADS quads.
     * Texture slots: up to MAX_TEXTURE_SLOTS simultaneous textures per batch.
     *
     * Usage:
     *          Renderer2D::Begin(camera);
     *          Renderer2D::DrawQuad({0,0}, {100,100}, {1,0,0,1});          // red quad
     *          Renderer2D::DrawSprite({200,0}, {64,64}, myTexture);       // textured sprite
     *          Renderer2D::End();
     *
     * Init() and Shutdown() are called by the RenderSubsystem — not by game code.
     */
    class OPAAX_API Renderer2D
    {
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        static void StartBatch();
        static void EmitFrame(); // sort the frame, then emit batches
        static void EmitBatch(Uint32 InQuadCount, Uint32 InSlotCount);

        //------------------------------------------------------------------------------
        
    public:
        static void Init();
        static void Shutdown();
        
        /**
         * Roll the per-frame stats: publish the frame just finished, zero the accumulator. Called once
         * per frame by the run loop (after RenderCommand::BeginFrame, before any pass draws).
         */
        static void NewFrame();

        /** Renderer counters for the previously completed frame (one frame late — see RenderStats). */
        static const RenderStats& GetStats();
     
        /**
         * Call once per frame (per pass) before any draw calls. Records into InCmd — binds the
         * sprite pipeline and writes the camera UBO; draws issued until End() record into InCmd too.
         * @param InCamera camera supplying the view-projection
         * @param InCmd    the frame's command buffer (from RenderContext)
         */
        static void Begin(ICamera& InCamera, ICommandBuffer& InCmd);

        /**
         * Call once per frame after all draw calls — flushes remaining batch
         */
        static void End();
     
        /**
         * Draw a solid-colour quad
         * @param InPosition centre of the quad (Y-up world space)
         * @param InSize full width and height
         * @param InColor RGBA normalised [0,1]
         * @param InRotationRad rotation around the quad centre, radians, CCW (default 0 = axis-aligned fast path)
         * @param InLayer coarse draw-order band (default Default)
         * @param InOrderInLayer fine tie-break within the band, lower = behind (default 0)
         */
        static void DrawQuad(const Vector2F& InPosition,
                             const Vector2F& InSize,
                             const Vector4F& InColor,
                             float           InRotationRad  = 0.f,
                             ERenderLayer    InLayer        = ERenderLayer::Default,
                             Int16           InOrderInLayer = 0);

        /**
         * Textured sprite — via AssetHandle (preferred, safe)
         */
        static void DrawSprite(const Vector2F&      InPosition,
                               const Vector2F&      InSize,
                               const TextureHandle& InTexture,
                               const Vector4F&      InColor        = Vector4F(1.f),
                               float                InRotationRad  = 0.f,
                               ERenderLayer         InLayer        = ERenderLayer::Default,
                               Int16                InOrderInLayer = 0);

        /**
         * Textured Atlas — via AssetHandle (preferred, safe)
         * Sprite sheet / atlas sub-region — UV in normalised [0,1] space
         */
        static void DrawSprite(const Vector2F&      InPosition,
                               const Vector2F&      InSize,
                               const TextureHandle& InTexture,
                               const Vector2F&      InUVMin,
                               const Vector2F&      InUVMax,
                               const Vector4F&      InColor        = Vector4F(1.f),
                               float                InRotationRad  = 0.f,
                               ERenderLayer         InLayer        = ERenderLayer::Default,
                               Int16                InOrderInLayer = 0);

        /**
         * Draw a textured sprite, tinted by InColor (default white = no tint)
         */
        static void DrawSprite(const Vector2F& InPosition,
                               const Vector2F& InSize,
                               Texture2D&      InTexture,
                               const Vector4F& InColor        = Vector4F(1.f),
                               float           InRotationRad  = 0.f,
                               ERenderLayer    InLayer        = ERenderLayer::Default,
                               Int16           InOrderInLayer = 0);

        /**
         * Draw a textured sprite with UV sub-region (sprite sheet / atlas)
         * InUVMin / InUVMax: normalised texture coordinates [0,1]
         */
        static void DrawSprite(const Vector2F& InPosition,
                               const Vector2F& InSize,
                               Texture2D&      InTexture,
                               const Vector2F& InUVMin,
                               const Vector2F& InUVMax,
                               const Vector4F& InColor        = Vector4F(1.f),
                               float           InRotationRad  = 0.f,
                               ERenderLayer    InLayer        = ERenderLayer::Default,
                               Int16           InOrderInLayer = 0);
    };
 
} // namespace Opaax