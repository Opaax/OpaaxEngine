#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"             // UniquePtr, Uint32
#include "Core/Maths/MathTypes.h"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/UI/IEditorUIBackend.h"

namespace Opaax
{
    class IFramebuffer;
    class OffscreenRenderTarget;
    
    OPAAX_LOG_CATEGORY(ViewportPanel)
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ViewportPanel — the dockable "Viewport" panel: the world is rendered into an offscreen FBO
    //   and shown as an ImGui image, so render resolution is decoupled from the window (D2). The
    //   panel OWNS the FBO + its OffscreenRenderTarget wrapper (I5), and registers the target as the
    //   engine's primary render target for its whole lifetime (Startup sets it, Shutdown clears it).
    //
    //   Resize is deferred by one frame to avoid reallocating the FBO between the world render and
    //   the sample within a single frame:
    //     Draw()        (end of frame N)   MEASURES the panel's content region -> caches a pending size.
    //     OnPreRender() (start of frame N+1) APPLIES the pending size BEFORE the world renders.
    //   The one-frame lag is normal for ImGui render-to-texture (not a bug).
    // =============================================================================
    class ViewportPanel final : public IEditorPanel
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit ViewportPanel(EditorContext& InContext);
        ~ViewportPanel() override;
        
        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        ViewportPanel(const ViewportPanel&)            = delete;
        ViewportPanel& operator=(const ViewportPanel&) = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * A null handle -> reserve space with a Dummy so the layout is unchanged (drawing a null texture is a backend validation error). 
         * @return Sample the FBO the world rendered into this frame. The backend yields the ImGui handle + the UVs that present it upright (GL FBOs are bottom-up). 
         */
        EditorViewportImage GetViewportImage() const;
        
        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        void            Startup()               override;
        void            OnPreRender()           override;
        void            Draw()                  override;
        void            Shutdown()              override;
        OpaaxStringID   GetPanelID()    const   override { return m_PanelID; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;
        
        const OpaaxStringID m_PanelID{ OPAAX_ID("Viewport") };
        const OpaaxString   m_Title = m_PanelID.ToString();

        UniquePtr<IFramebuffer>          m_Framebuffer;
        UniquePtr<OffscreenRenderTarget> m_RenderTarget;
        
        Vector2F   m_viewportSizeDefault  = {960.f, 600.f};
        Vector2u32 m_viewportSize         = {1,1};
        Vector2u32 m_viewportPendingSize  = {1,1};
        
        bool   m_bImageLogged  = false;
    };
}
