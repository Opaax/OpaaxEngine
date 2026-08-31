#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"    // DrawBounds — the form every caller already holds
#include "Core/Maths/MathTypes.h"
#include "Renderer/RenderLayer.h"   // ERenderLayer — a segment states its own band

namespace Opaax
{
    /**
     * One queued debug segment, in world units. Plain data — no GPU state, no lifetime.
     */
    struct DebugLine
    {
        Vector2F Start     = { 0.f, 0.f };
        Vector2F End       = { 0.f, 0.f };
        Vector4F Color     = { 1.f, 1.f, 1.f, 1.f };
        float    Thickness = 1.f;

        /**
         * WHICH BAND the segment draws in. Debug — above all world geometry — is right for an
         * overlay that must not be hidden by what it annotates, and it is the only thing this queue
         * could express until ③b needed a BACKGROUND grid: a grid drawn over every sprite is not a
         * grid, it is a cage.
         */
        ERenderLayer Layer = ERenderLayer::Debug;
    };

    /**
     * One queued rectangle OUTLINE, in world units. Its own entry rather than four DebugLines,
     * because it renders as ONE hollow quad (Renderer2D::DrawQuadOutline) — the whole point of
     * having it: a selection of N entities costs N quads instead of 4N.
     */
    struct DebugBox
    {
        Vector2F Center    = { 0.f, 0.f };
        Vector2F Size      = { 0.f, 0.f };   // FULL width and height, border included
        Vector4F Color     = { 1.f, 1.f, 1.f, 1.f };
        float    Thickness = 1.f;

        ERenderLayer Layer = ERenderLayer::Debug;
    };

    /**
     * The oriented thin quad that covers a DebugLine — the form Renderer2D::DrawQuad consumes.
     * Size is { segment length, line thickness }; RotationRad turns it onto the segment.
     */
    struct DebugQuad
    {
        Vector2F Center      = { 0.f, 0.f };
        Vector2F Size        = { 0.f, 0.f };
        float    RotationRad = 0.f;
    };

    /**
     * Line -> thin rotated quad. Pure geometry, so the whole line rendering path is unit-testable
     * without a GL context. A zero-length segment yields Size.x == 0 (nothing drawn) and rotation 0
     * — never NaN.
     */
    OPAAX_API DebugQuad ToQuad(const DebugLine& InLine) noexcept;

    /**
     * @class DebugDraw
     *
     * Per-frame debug line queue (Editor.md D10). Producers enqueue during the frame; the renderer
     * drains it once, at Render, and clears it — nothing survives to the next frame, so a caller
     * re-submits every frame it wants a line visible (immediate-mode, matching the editor's UI).
     *
     * Engine-owned, NOT editor-owned: it serves editor overlays AND dev builds of Game.exe, which
     * never links OpaaxEditorLib. Owned by value by RendererManager — the thing that drains it —
     * and reached through IEngine::GetDebugDraw().
     *
     * Lines and BOXES. Everything else (circles, text, persistent durations) waits for a caller that
     * needs it. A line becomes a thin rotated quad; a box becomes ONE hollow quad rather than four
     * of them (F4d) — still one pipeline and one batch, which is the property worth keeping.
     */
    class OPAAX_API DebugDraw
    {
        // =============================================================================
        // Draw calls
        // =============================================================================
    public:
        /**
         * Queue a segment.
         * @param InStart segment start, world units
         * @param InEnd segment end, world units
         * @param InColor RGBA normalised [0,1]
         * @param InThickness line width in world units (1 unit = 1px at the render target's native size)
         * @param InLayer draw band; the default keeps every existing caller above world geometry
         */
        void DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor,
                      float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug);

        /**
         * Queue an axis-aligned rectangle outline as ONE hollow quad.
         *
         * It used to be four segments; a border is now a property of a quad rather than four thin
         * ones laid end to end, so this costs a quarter of the geometry and the corners are exact
         * instead of overlapping by half a line width.
         *
         * @param InCenter rectangle centre, world units
         * @param InSize full width and height, border included
         * @param InColor RGBA normalised [0,1]
         * @param InThickness border width in world units. Thick enough to close the middle draws solid.
         * @param InLayer draw band
         */
        void DrawBox(const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor,
                     float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug);

        /**
         * The same box, taking the shape every caller already has.
         *
         * `EntityQuery::TryGetBounds` answers a Bounds2D and each call site was unpacking it into a
         * centre and a size just to hand both back; this is the one that should be reached for.
         */
        void DrawBounds(const Bounds2D& InBounds, const Vector4F& InColor,
                        float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug);

        // =============================================================================
        // Consumption — the renderer's side
        // =============================================================================
    public:
        /*** Everything queued since the last Clear(), in submission order. */
        const TDynArray<DebugLine>& GetLines() const noexcept { return m_Lines; }

        /*** Boxes queued since the last Clear(). Drained beside the lines, one quad each. */
        const TDynArray<DebugBox>& GetBoxes() const noexcept { return m_Boxes; }

        /*** Drop BOTH queues. Called once per frame by the owner, drawn or not. */
        void Clear() noexcept;

        bool IsEmpty() const noexcept { return m_Lines.empty() && m_Boxes.empty(); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<DebugLine> m_Lines;
        TDynArray<DebugBox>  m_Boxes;
    };
}
