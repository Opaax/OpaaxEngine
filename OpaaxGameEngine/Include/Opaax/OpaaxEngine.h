#pragma once
#include "OpaaxNonCopyableAndMovable.h"
#include "Renderer/OpaaxBackendRenderer.h"

namespace OPAAX
{
    class OPAAX_API OpaaxEngine : public OpaaxNonCopyableAndMovable
    {
    private:
        RENDERER::EOPBackendRenderer m_renderer;
        
    public:
        OpaaxEngine();
        ~OpaaxEngine() override = default;

        void LoadConfig();
        void Init();

        static OpaaxEngine& Get();

        FORCEINLINE RENDERER::EOPBackendRenderer& GetRenderer() { return m_renderer; }
    };
}
