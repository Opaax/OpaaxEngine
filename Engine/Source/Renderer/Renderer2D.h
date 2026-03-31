#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Camera2D.h"
 
#include <glm/glm.hpp>
 
namespace Opaax
{
    class OpenGLTexture2D;
    
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
    public:
        static void Init();
        static void Shutdown();
 
        // Call once per frame before any draw calls
        static void Begin(Camera2D& InCamera);
 
        // Call once per frame after all draw calls — flushes remaining batch
        static void End();
 
        // Draw a solid-colour quad
        // InPosition: centre of the quad (Y-up world space)
        // InSize:     full width and height
        // InColor:    RGBA normalised [0,1]
        static void DrawQuad(const Vector2F& InPosition,
                             const Vector2F& InSize,
                             const Vector4F& InColor);
 
        // Draw a textured sprite, tinted by InColor (default white = no tint)
        static void DrawSprite(const Vector2F&     InPosition,
                               const Vector2F&     InSize,
                               OpenGLTexture2D&     InTexture,
                               const Vector4F&     InColor = Vector4F(1.f));
 
        // Draw a textured sprite with UV sub-region (sprite sheet / atlas)
        // InUVMin / InUVMax: normalised texture coordinates [0,1]
        static void DrawSprite(const Vector2F&     InPosition,
                               const Vector2F&     InSize,
                               OpenGLTexture2D&     InTexture,
                               const Vector2F&     InUVMin,
                               const Vector2F&     InUVMax,
                               const Vector4F&     InColor = Vector4F(1.f));
 
    private:
        static void Flush();
        static void StartBatch();
        static float GetTextureSlot(OpenGLTexture2D& InTexture);
    };
 
} // namespace Opaax