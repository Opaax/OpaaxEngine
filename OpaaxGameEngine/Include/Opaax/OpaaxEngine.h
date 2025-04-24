#pragma once
#include "OpaaxNonCopyableAndMovable.h"
#include "Input/OpaaxInputSystem.h"
#include "Renderer/OpaaxBackendRenderer.h"

namespace OPAAX
{
    class OPAAX_API OpaaxEngine : public OpaaxNonCopyableAndMovable
    {
    private:
        RENDERER::EOPBackendRenderer m_renderer;

        OpaaxInputSystem m_input;
        
    public:
        OpaaxEngine();
        ~OpaaxEngine() override = default;

        void LoadConfig();
        void Init();

        static OpaaxEngine& Get();

        FORCEINLINE RENDERER::EOPBackendRenderer& GetRenderer() { return m_renderer; }
        FORCEINLINE OpaaxInputSystem& GetInput() { return m_input; }
    };
}
