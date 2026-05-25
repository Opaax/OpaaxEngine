#include "OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Opaax
{
    void OrthographicCamera::SetPosition(const Vector2F& InPosition)
    {
        if (m_Position != InPosition)
        {
            m_Position = InPosition;
            m_bDirty   = true;
        }
    }

    void OrthographicCamera::SetZoom(float InZoom)
    {
        if (m_Zoom != InZoom)
        {
            m_Zoom   = InZoom;
            m_bDirty = true;
        }
    }

    const Matrix44F& OrthographicCamera::GetViewProjection()
    {
        if (m_bDirty)
        {
            RecalculateViewProjection();
        }
        return m_ViewProjectionMatrix;
    }

    void OrthographicCamera::SetViewportSize(Uint32 InWidth, Uint32 InHeight)
    {
        if (m_ViewportWidth == InWidth && m_ViewportHeight == InHeight)
        {
            return;
        }

        m_ViewportWidth  = InWidth;
        m_ViewportHeight = InHeight;
        m_bDirty         = true;
    }

    void OrthographicCamera::RecalculateViewProjection()
    {
        const float lHalfW = (static_cast<float>(m_ViewportWidth)  * 0.5f) / m_Zoom;
        const float lHalfH = (static_cast<float>(m_ViewportHeight) * 0.5f) / m_Zoom;

        // Y-up orthographic: bottom = -halfH, top = +halfH.
        m_ProjectionMatrix = glm::ortho(
            -lHalfW, lHalfW,
            -lHalfH, lHalfH,
            -1.f, 1.f);

        // View matrix: translate by -position so the world moves opposite to the camera.
        m_ViewMatrix = glm::translate(
            glm::mat4(1.f),
            glm::vec3(-m_Position.x, -m_Position.y, 0.f));

        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
        m_bDirty               = false;
    }

} // namespace Opaax
