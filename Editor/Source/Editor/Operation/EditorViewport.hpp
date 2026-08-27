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
    //   Only the SIZE. Hover and focus are InputRoute's, because they answer a different question
    //   ("is the engine being fed") that has its own consumer and its own rule.
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
        // Members
        // =============================================================================
    private:
        Vector2F m_SizePx = { 0.f, 0.f };
    };
}
