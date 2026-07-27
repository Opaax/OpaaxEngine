#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

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
     * Lines only. Everything else (circles, text, persistent durations) waits for a caller that
     * needs it. Rendering costs no new RHI surface: each line becomes a thin rotated quad through
     * the existing Renderer2D::DrawQuad.
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
         */
        void DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor,
                      float InThickness = 1.f);

        /**
         * Queue an axis-aligned rectangle outline as four segments.
         * @param InCenter rectangle centre, world units
         * @param InSize full width and height
         * @param InColor RGBA normalised [0,1]
         * @param InThickness line width in world units
         */
        void DrawBox(const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor,
                     float InThickness = 1.f);

        // =============================================================================
        // Consumption — the renderer's side
        // =============================================================================
    public:
        /*** Everything queued since the last Clear(), in submission order. */
        const TDynArray<DebugLine>& Lines() const noexcept { return m_Lines; }

        /*** Drop the queue. Called once per frame by the owner, drawn or not. */
        void Clear() noexcept;

        bool IsEmpty() const noexcept { return m_Lines.empty(); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<DebugLine> m_Lines;
    };
}
