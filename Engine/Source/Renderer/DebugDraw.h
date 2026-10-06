#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxStringID.hpp"   // DebugChannel
#include "Renderer/RenderLayer.h"   // ERenderLayer

namespace Opaax
{
    class World;

    // =============================================================================
    // Channels — groups of debug shapes that can be switched off
    // =============================================================================
    /**
     * A producer's name, so its shapes can be hidden from outside.
     */
    using DebugChannel = OpaaxStringID;

    /**
     * The engine's channels. Use these constants (a typo in a string would silently never match).
     */
    namespace DebugChannels
    {
        /** Anything without a channel: grid, selection outlines, gizmo helpers. */
        inline const DebugChannel Default = OPAAX_ID("Default");

        /** Collider outlines. */
        inline const DebugChannel Physics = OPAAX_ID("Physics");
    }

    /**
     * One queued debug line, in world units.
     */
    struct DebugLine
    {
        Vector2F Start     = { 0.f, 0.f };
        Vector2F End       = { 0.f, 0.f };
        Vector4F Color     = { 1.f, 1.f, 1.f, 1.f };
        float    Thickness = 1.f;

        /**
         * Draw layer. Debug (above the world) by default; the editor grid uses Background.
         */
        ERenderLayer Layer = ERenderLayer::Debug;

        /** The world this line belongs to. Null = the active world. */
        const World* Source = nullptr;
    };

    /**
     * One queued rectangle outline, in world units (drawn as one hollow quad).
     */
    struct DebugBox
    {
        Vector2F Center    = { 0.f, 0.f };
        Vector2F Size      = { 0.f, 0.f };   // full width and height, border included
        Vector4F Color     = { 1.f, 1.f, 1.f, 1.f };
        float    Thickness = 1.f;

        /**
         * Rotation about the centre, radians, counter-clockwise.
         */
        float RotationRad = 0.f;

        ERenderLayer Layer  = ERenderLayer::Debug;
        const World* Source = nullptr;
    };

    /**
     * The thin rotated quad covering a DebugLine: Size is { length, thickness }.
     */
    struct DebugQuad
    {
        Vector2F Center      = { 0.f, 0.f };
        Vector2F Size        = { 0.f, 0.f };
        float    RotationRad = 0.f;
    };

    /**
     * Line -> thin rotated quad. A zero-length line gives Size.x == 0 and rotation 0 (never NaN).
     */
    DebugQuad ToQuad(const DebugLine& InLine) noexcept;

    // =============================================================================
    // Outline geometry (pure functions)
    // =============================================================================
    /**
     * A circle as InSegments points, each InRadius from InCenter. OutPoints is cleared first.
     * The last point connects back to the first.
     */
    void BuildCircleOutline(Vector2F InCenter, float InRadius, Uint32 InSegments,
                                      TDynArray<Vector2F>& OutPoints);

    /**
     * A capsule as a closed polygon: a half-turn of InSegmentsPerCap points around each end,
     * joined by the flanks. OutPoints is cleared first. Equal centres give a circle.
     */
    void BuildCapsuleOutline(Vector2F InCenter1, Vector2F InCenter2, float InRadius,
                                       Uint32 InSegmentsPerCap, TDynArray<Vector2F>& OutPoints);

    /**
     * Per-frame debug shape queue (lines, boxes, circles, capsules). Cleared every frame by the
     * renderer: submit every frame to keep a shape visible. Works in the editor and in dev game
     * builds. Owned by RendererManager; reached through IEngine::GetDebugDraw().
     *
     * Every shape has a channel (disabled channels are dropped on submit) and a world
     * (null = the active world; a pass only draws its own world's shapes).
     */
    class DebugDraw
    {
        // =============================================================================
        // Draw calls
        // =============================================================================
    public:
        /**
         * Queues a line.
         * @param InStart Start, world units
         * @param InEnd End, world units
         * @param InColor RGBA [0,1]
         * @param InThickness Width in world units
         * @param InLayer Draw layer (default: above the world)
         * @param InSource The world it belongs to; null = the active world
         */
        void DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor,
                      float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                      DebugChannel InChannel = DebugChannels::Default, const World* InSource = nullptr);

        /**
         * Queues an axis-aligned rectangle outline (one hollow quad).
         * @param InCenter Centre, world units
         * @param InSize Full width and height, border included
         * @param InColor RGBA [0,1]
         * @param InThickness Border width in world units (thick enough fills it)
         * @param InLayer Draw layer
         */
        void DrawBox(const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor,
                     float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                     DebugChannel InChannel = DebugChannels::Default, float InRotationRad = 0.f,
                     const World* InSource = nullptr);

        /**
         * Same, from a Bounds2D.
         */
        void DrawBounds(const Bounds2D& InBounds, const Vector4F& InColor,
                        float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                        DebugChannel InChannel = DebugChannels::Default, const World* InSource = nullptr);

        /**
         * Queues a circle outline.
         * @param InSegments Number of sides
         */
        void DrawCircle(const Vector2F& InCenter, float InRadius, const Vector4F& InColor,
                        float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                        DebugChannel InChannel = DebugChannels::Default, Uint32 InSegments = 24,
                        const World* InSource = nullptr);

        /**
         * Queues a capsule outline. InCenter1/InCenter2 are the cap centres, in world space.
         */
        void DrawCapsule(const Vector2F& InCenter1, const Vector2F& InCenter2, float InRadius,
                         const Vector4F& InColor, float InThickness = 1.f,
                         ERenderLayer InLayer = ERenderLayer::Debug,
                         DebugChannel InChannel = DebugChannels::Default,
                         Uint32 InSegmentsPerCap = 12, const World* InSource = nullptr);

        // =============================================================================
        // Channels
        // =============================================================================
    public:
        /** Hides or shows a channel. */
        void SetChannelEnabled(DebugChannel InChannel, bool bInEnabled);

        /** True unless disabled. Unknown channels are drawn. */
        bool IsChannelEnabled(DebugChannel InChannel) const noexcept;

        // =============================================================================
        // Renderer side
        // =============================================================================
    public:
        /** Lines queued since the last Clear(), in order. */
        const TDynArray<DebugLine>& GetLines() const noexcept { return m_Lines; }

        /** Boxes queued since the last Clear(). */
        const TDynArray<DebugBox>& GetBoxes() const noexcept { return m_Boxes; }

        /** Clears both queues. Called once per frame. */
        void Clear() noexcept;

        bool IsEmpty() const noexcept { return m_Lines.empty() && m_Boxes.empty(); }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** Queues m_OutlineScratch as a closed loop of lines. */
        void EmitClosedPolygon(const Vector4F& InColor, float InThickness, ERenderLayer InLayer,
                               const World* InSource);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<DebugLine> m_Lines;
        TDynArray<DebugBox>  m_Boxes;

        /**
         * Disabled channels (so a new channel is visible by default). Not cleared by Clear().
         */
        TUnorderedSet<Uint32> m_DisabledChannels;

        /** Reused by the outline builders. */
        TDynArray<Vector2F> m_OutlineScratch;
    };
}
