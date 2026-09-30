#pragma once

#include "Core/Maths/MathTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorViewport — the viewport image size in pixels, and the grid toggle. Written by the
    //   ViewportPanel / toolbar, read outside the panel (focus-selected needs the aspect).
    //   Hover and focus are InputRoute's.
    // =============================================================================
    class EditorViewport
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void SetSizePx(const Vector2F& InSizePx) noexcept { m_SizePx = InSizePx; }

        /** The image size in pixels. {0,0} until the panel has drawn once (check IsValid). */
        const Vector2F& GetSizePx() const noexcept { return m_SizePx; }

        /** False before the first measurement, and for a collapsed panel. */
        bool IsValid() const noexcept { return m_SizePx.x > 1.f && m_SizePx.y > 1.f; }

        // =============================================================================
        // Overlays
        // =============================================================================
    public:
        /** The snap grid. Off by default. */
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
