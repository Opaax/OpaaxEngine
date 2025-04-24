#pragma once
#include "OpaaxNonCopyableAndMovable.h"
#include "Input/OpaaxInputSystem.h"
#include "Renderer/OpaaxBackendRenderer.h"
#include "Imgui/OpaaxImguiBase.h"

namespace OPAAX
{
    namespace IMGUI {
        class OpaaxImguiBase;
    }

    class OPAAX_API OpaaxEngine : public OpaaxNonCopyableAndMovable
    {
    private:
        RENDERER::EOPBackendRenderer m_renderer;

        TUniquePtr<IMGUI::OpaaxImguiBase> m_imgui;

        OpaaxInputSystem m_input;
        
    public:
        OpaaxEngine();
        ~OpaaxEngine() override = default;

    private:
        void CreateImgui();
        
    public:
        void LoadConfig();
        void Initialize();

        static OpaaxEngine& Get();

        FORCEINLINE RENDERER::EOPBackendRenderer&   GetRenderer()       { return m_renderer; }
        FORCEINLINE OpaaxInputSystem&               GetInput()          { return m_input; }
        FORCEINLINE IMGUI::OpaaxImguiBase&          GetImgui() const    { return *m_imgui; }
    };
}
