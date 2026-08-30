#pragma once

#include "Core/Maths/MathTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorViewport — how big the viewport's image is, in pixels. Written by ViewportPanel every
    //   frame (it already measures the content region for its deferred resize), read by anything
    //   outside the panel that has to turn world units into screen ones or back.
    //
    //   Here rather than on the panel for the reason ResourcePreview is (I16): the WRITER is a
    //   panel and the READER is a menu command — focus-selected needs the ASPECT to frame a wide
    //   selection, and EditorPanels hands out IEditorPanel with no typed getter, by design.
    //
    //   The size, and what is DRAWN OVER it. Hover and focus are still InputRoute's, because they
    //   answer a different question ("is the engine being fed") with its own consumer and its own
    //   rule — that separation is unchanged.
    //   *This read "Only the SIZE" until ③b: the grid toggle is written by a TOOLBAR ITEM and read
    //   by the panel, so it is the same writer-and-reader-are-different-objects case as the size.*
    // =============================================================================
    class EditorViewport
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void SetSizePx(const Vector2F& InSizePx) noexcept { m_SizePx = InSizePx; }

        /** The image's pixel size. {0,0} until the panel has drawn once — check IsValid first. */
        const Vector2F& GetSizePx() const noexcept { return m_SizePx; }

        /** False before the first real measurement, and for a collapsed panel. */
        bool IsValid() const noexcept { return m_SizePx.x > 1.f && m_SizePx.y > 1.f; }

        // =============================================================================
        // Overlays
        // =============================================================================
    public:
        /** The snap grid (③b) — off by default, because an empty map reads better without it. */
        void SetShowGrid(const bool bInShow) noexcept { m_bShowGrid = bInShow; }
        bool IsGridVisible() const noexcept           { return m_bShowGrid; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Vector2F m_SizePx    = { 0.f, 0.f };
        bool     m_bShowGrid = false;
    };
}
