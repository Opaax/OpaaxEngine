#include "ScreenSpaceCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Opaax
{
    void ScreenSpaceCamera::Recalculate()
    {
        // Bottom-left origin, Y-up, pixel units. View matrix is identity (no position),
        // so ViewProjection == projection.
        m_ViewProjection = glm::ortho(
            0.f, static_cast<float>(m_Width),
            0.f, static_cast<float>(m_Height),
            -1.f, 1.f);
        m_bDirty = false;
    }

    const Matrix44F& ScreenSpaceCamera::GetViewProjection()
    {
        if (m_bDirty) { Recalculate(); }
        return m_ViewProjection;
    }

    void ScreenSpaceCamera::SetViewportSize(Uint32 InWidth, Uint32 InHeight)
    {
        if (InWidth == m_Width && InHeight == m_Height) { return; }
        m_Width  = InWidth;
        m_Height = InHeight;
        m_bDirty = true;
    }
}
