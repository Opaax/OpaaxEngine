#include "Camera2D.h"

#include "Core/Log/OpaaxLog.h"
 
#include <glm/gtc/matrix_transform.hpp>

#include "Core/CoreEngineApp.h"
#include "Core/Window.h"
#include "Core/Event/OpaaxEventDispatcher.hpp"

namespace Opaax
{
    bool Camera2D::Startup()
    {
        OPAAX_CORE_INFO("Camera2D::Startup()");
 
        // Fetch initial viewport from the window if the engine app is available
        if (GetEngineApp())
        {
            const auto& lWindow = GetEngineApp()->GetWindow();
            SetViewportSize(lWindow.GetWidth(), lWindow.GetHeight());
        }
 
        RecalculateViewProjection();
        return true;
    }
 
    void Camera2D::Shutdown()
    {
        OPAAX_CORE_INFO("Camera2D::Shutdown()");
    }
 
    bool Camera2D::OnEvent(OpaaxEvent& Event)
    {
        OpaaxEventDispatcher lDispatcher(Event);
        lDispatcher.Dispatch<WindowResizeEvent>(
            [this](WindowResizeEvent& Event) { return OnWindowResize(Event); }
            );
        
        return false;
    }
 
    void Camera2D::SetPosition(const Vector2F& InPosition)
    {
        if (m_Position != InPosition)
        {
            m_Position = InPosition;
            m_bDirty   = true;
        }
    }
 
    void Camera2D::SetZoom(float InZoom)
    {
        if (m_Zoom != InZoom)
        {
            m_Zoom   = InZoom;
            m_bDirty = true;
        }
    }
 
    const Matrix44F& Camera2D::GetViewProjection()
    {
        if (m_bDirty)
        {
            RecalculateViewProjection();
        }
        return m_ViewProjectionMatrix;
    }
    
    void Camera2D::RecalculateViewProjection()
    {
        const float lHalfW = (static_cast<float>(m_ViewportWidth)  * 0.5f) / m_Zoom;
        const float lHalfH = (static_cast<float>(m_ViewportHeight) * 0.5f) / m_Zoom;
 
        // Y-up orthographic: bottom = -halfH, top = +halfH
        m_ProjectionMatrix = glm::ortho(
            -lHalfW, lHalfW,
            -lHalfH, lHalfH,
            -1.f, 1.f);
 
        // View matrix: inverse of the camera transform
        // Translate by -position so objects move opposite to the camera
        m_ViewMatrix = glm::translate(
            glm::mat4(1.f),
            glm::vec3(-m_Position.x, -m_Position.y, 0.f));
 
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
        m_bDirty               = false;
    }
 
    void Camera2D::SetViewportSize(Uint32 InWidth, Uint32 InHeight)
    {
        if (m_ViewportWidth == InWidth && m_ViewportHeight == InHeight) { return; }
 
        m_ViewportWidth  = InWidth;
        m_ViewportHeight = InHeight;
        m_bDirty         = true;
    }
 
    bool Camera2D::OnWindowResize(WindowResizeEvent& Event)
    {
        SetViewportSize(Event.GetWidth(), Event.GetHeight());
        return false;  // not consumed — other systems may also want resize
    }
 
} // namespace Opaax