#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"    // DrawBounds — the form every caller already holds
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxStringID.hpp"   // DebugChannel — an already-interned, already-exported type
#include "Renderer/RenderLayer.h"   // ERenderLayer — a segment states its own band

namespace Opaax
{
    class World;

    // =============================================================================
    // Channels — what a central toggle switches off (F4b)
    // =============================================================================
    /**
     * A producer's name, so a consumer can silence it from OUTSIDE. `OpaaxStringID` exactly as F4b
     * specifies: already `OPAAX_API`, already interned, and interned out-of-line in the DLL (I2),
     * so an id agrees across the module boundary by construction — it adds no static of its own.
     */
    using DebugChannel = OpaaxStringID;

    /**
     * The channels the engine itself produces on. A named constant rather than a bare `OPAAX_ID`
     * at each call site, because the producer and the toggle agreeing on a STRING is precisely the
     * thing that fails silently — a typo would simply never switch anything off.
     */
    namespace DebugChannels
    {
        /** Everything that never named a channel. Grid, selection outlines, gizmo helpers. */
        inline const DebugChannel Default = OPAAX_ID("Default");

        /** Collider outlines. ⑦-A P3 — the second producer, which is what earned this whole idea. */
        inline const DebugChannel Physics = OPAAX_ID("Physics");
    }

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

        /** WHICH WORLD the segment annotates. Null is the ACTIVE one — see the class note. */
        const World* Source = nullptr;
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

        /**
         * Rotation about the centre, radians, CCW. `Renderer2D::DrawQuadOutline` has always taken
         * one and this queue simply never carried it, passing a hardcoded 0 at the drain — which a
         * ROTATED collider made visible (⑦-A P3): its outline stayed axis-aligned while the shape
         * it annotated did not.
         */
        float RotationRad = 0.f;

        ERenderLayer Layer  = ERenderLayer::Debug;
        const World* Source = nullptr;
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

    // =============================================================================
    // Outline geometry — pure, and therefore the testable half
    // =============================================================================
    /**
     * The closed polygon approximating a circle: InSegments points, evenly spaced, each exactly
     * InRadius from InCenter. OutPoints is cleared first. The closing edge is implied — the caller
     * joins last to first — so the point count IS the segment count.
     *
     * Free and pure for `ToQuad`'s reason: a circle drawn as segments needs no new RHI primitive,
     * and the part that can be wrong is arithmetic a test can hold without a GL context.
     */
    OPAAX_API void BuildCircleOutline(Vector2F InCenter, float InRadius, Uint32 InSegments,
                                      TDynArray<Vector2F>& OutPoints);

    /**
     * The closed polygon approximating a capsule: a half-turn of InSegmentsPerCap points around
     * each end centre, joined by the two straight flanks. Every point is exactly InRadius from the
     * cap centre it belongs to. OutPoints is cleared first; the closing edge is implied.
     *
     * A degenerate capsule (the two centres equal) is a circle, and comes out as one.
     */
    OPAAX_API void BuildCapsuleOutline(Vector2F InCenter1, Vector2F InCenter2, float InRadius,
                                       Uint32 InSegmentsPerCap, TDynArray<Vector2F>& OutPoints);

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
     * Lines, BOXES, and — since ⑦-A P3 gave them a caller — circles and capsules, which are
     * polygons of lines rather than queue entries of their own: no new drain path, no new RHI
     * primitive, and the renderer never learns they exist. Text and persistent durations still
     * wait. A line becomes a thin rotated quad; a box becomes ONE hollow quad rather than four of
     * them (F4d) — still one pipeline and one batch, which is the property worth keeping.
     *
     * EVERY SUBMISSION CARRIES A CHANNEL, and a disabled one is dropped HERE rather than at the
     * drain (F4b): "stop calling" cannot be asked of a producer by a toggle that lives outside it,
     * and filtering at submit means a silenced channel costs one lookup instead of memory it will
     * never render. A channel nobody has touched is ENABLED — a new producer is visible by
     * default, which is the right way round for debug output.
     *
     * EVERY SUBMISSION NAMES ITS WORLD, and null means the ACTIVE one (⑦-C P8 — the rule a render
     * view already follows, one layer down). The queue is one list per frame and a pass drains only
     * the primitives of the world it draws; without the tag a second edited world would receive the
     * level's grid in its own coordinates and hand its outline back. No producer that existed
     * before had to change: only the active world ticks, so null was always what they meant.
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
         * @param InSource the world this annotates; null is the active one
         */
        void DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor,
                      float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                      DebugChannel InChannel = DebugChannels::Default, const World* InSource = nullptr);

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
                     float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                     DebugChannel InChannel = DebugChannels::Default, float InRotationRad = 0.f,
                     const World* InSource = nullptr);

        /**
         * The same box, taking the shape every caller already has.
         *
         * `EntityQuery::TryGetBounds` answers a Bounds2D and each call site was unpacking it into a
         * centre and a size just to hand both back; this is the one that should be reached for.
         */
        void DrawBounds(const Bounds2D& InBounds, const Vector4F& InColor,
                        float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                        DebugChannel InChannel = DebugChannels::Default, const World* InSource = nullptr);

        /**
         * Queue a circle OUTLINE as a closed polygon of segments.
         *
         * @param InSegments how many sides approximate it. The default reads round at any zoom a
         *   2D editor reaches; fewer is legitimate for something small or numerous.
         */
        void DrawCircle(const Vector2F& InCenter, float InRadius, const Vector4F& InColor,
                        float InThickness = 1.f, ERenderLayer InLayer = ERenderLayer::Debug,
                        DebugChannel InChannel = DebugChannels::Default, Uint32 InSegments = 24,
                        const World* InSource = nullptr);

        /**
         * Queue a capsule OUTLINE — two half-turn caps joined by their flanks — as one closed
         * polygon. InCenter1/InCenter2 are the cap centres in WORLD space, so a caller that has a
         * local capsule offsets it first.
         */
        void DrawCapsule(const Vector2F& InCenter1, const Vector2F& InCenter2, float InRadius,
                         const Vector4F& InColor, float InThickness = 1.f,
                         ERenderLayer InLayer = ERenderLayer::Debug,
                         DebugChannel InChannel = DebugChannels::Default,
                         Uint32 InSegmentsPerCap = 12, const World* InSource = nullptr);

        // =============================================================================
        // Channels — the central toggle (F4b)
        // =============================================================================
    public:
        /** Silence or restore a whole producer. Enabling one nobody disabled is a no-op. */
        void SetChannelEnabled(DebugChannel InChannel, bool bInEnabled);

        /** True unless something explicitly disabled it — an unknown channel draws. */
        bool IsChannelEnabled(DebugChannel InChannel) const noexcept;

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
        // Internal
        // =============================================================================
    private:
        /** Queue m_OutlineScratch as a closed loop of segments. The channel is already checked. */
        void EmitClosedPolygon(const Vector4F& InColor, float InThickness, ERenderLayer InLayer,
                               const World* InSource);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<DebugLine> m_Lines;
        TDynArray<DebugBox>  m_Boxes;

        /**
         * The channels somebody switched OFF, keyed by the id's integer. A disabled-set rather
         * than an enabled-set, so the default answer is "draws": a producer added later is
         * visible without anyone having to register it first.
         *
         * Not cleared by Clear() — a toggle is a setting, not per-frame state.
         */
        TUnorderedSet<Uint32> m_DisabledChannels;

        /** Scratch for the outline builders, so a per-frame circle allocates nothing. */
        TDynArray<Vector2F> m_OutlineScratch;
    };
}
