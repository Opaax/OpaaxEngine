#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorViewport — the viewport image size in pixels, the grid toggle, and the selection outline
    //   drawn. Written by the ViewportPanel / toolbar, read outside the panel (focus-selected needs
    //   the aspect, automation the outline). Hover and focus are InputRoute's.
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

        /** The selection outline's box on the last frame drawn (world units); none when nothing was outlined. */
        void SetOutlined(const TOptional<Bounds2D>& InBounds) noexcept { m_Outlined = InBounds; }
        const TOptional<Bounds2D>& GetOutlined() const noexcept     { return m_Outlined; }

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

        TOptional<Bounds2D> m_Outlined;
    };
}
