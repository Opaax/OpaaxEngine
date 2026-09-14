#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Renderer/CameraView.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class Renderer2D;

    /** What one Update cost. Zero on an idle frame is the number the design owes. */
    struct UICanvasStats
    {
        Uint32 Layouts  = 0;   // rects resolved
        Uint32 Rebuilds = 0;   // quad lists rebuilt
    };

    // =============================================================================
    // UICanvas — a widget tree and the space it lives in.
    //
    //   CANVAS UNITS ARE REFERENCE PIXELS. The canvas is ReferenceHeight tall, centred on the
    //   origin, and as wide as the target's aspect makes it — its view is a CameraView whose
    //   OrthoSize is half the reference height, so CAM2 does the scaling: a same-aspect resize
    //   changes the projection and nothing in canvas space; only an aspect change moves a rect,
    //   and only the anchored ones.
    //
    //   Owned by whoever hosts it. The host calls SetTargetSize + Update once a frame, Submit
    //   inside its pass, HitTest with a point from ScreenToCanvas.
    // =============================================================================
    class OPAAX_API UICanvas
    {
        // =============================================================================
        // CTOR / DTOR
        // =============================================================================
    public:
        explicit UICanvas(float InReferenceHeight = 1080.f);

        UICanvas(const UICanvas&)            = delete;
        UICanvas& operator=(const UICanvas&) = delete;

        // =============================================================================
        // Space
        // =============================================================================
    public:
        float GetReferenceHeight() const noexcept { return m_ReferenceHeight; }
        void  SetReferenceHeight(float InHeight);

        /** The pixels this canvas is shown in. Same visible rect as last time dirties nothing. */
        void SetTargetSize(Uint32 InWidth, Uint32 InHeight);

        /** The canvas rect the target currently shows — the root's rect. */
        const Bounds2D& GetVisibleBounds() const noexcept { return m_VisibleBounds; }

        /** The view a pass composes this canvas with: origin-centred, half the reference height. */
        CameraView MakeView() const noexcept;

        /** A pixel in the target → canvas units, through the ONE screen→world rule. */
        Vector2F ScreenToCanvas(const Vector2F& InPixel) const noexcept;

        // =============================================================================
        // Tree
        // =============================================================================
    public:
        UIWidget&       Root()       noexcept { return *m_Root; }
        const UIWidget& Root() const noexcept { return *m_Root; }

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        /** Resolve and rebuild what is dirty — one walk, once a frame. */
        UICanvasStats Update(const UIBuildContext& InContext = {});

        /** Every visible widget's quads, tree order, into the open pass. */
        void Submit(Renderer2D& InRenderer) const;

        /** The top-most hit-testable widget under InCanvasPoint, or null. */
        const UIWidget* HitTest(const Vector2F& InCanvasPoint) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        float    m_ReferenceHeight;
        Uint32   m_TargetWidth  = 0;
        Uint32   m_TargetHeight = 0;
        Bounds2D m_VisibleBounds;
        bool     m_bVisibleChanged = true;

        TUniquePtr<UIWidget> m_Root;
        mutable bool         m_bWarnedOrderOverflow = false;
    };
}
