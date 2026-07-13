#include "RendererManager.h"

#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/Window/IWindowManager.h"
#include "Core/Application/Services/IConfigSystem.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Config/Config_Engine.h"
#include "Core/Config/Config_Renderer.h"

#include "RHI/RenderAPI.h"        // BackendFromString
#include "RHI/RenderLog.h"        // ERenderLogLevel
#include "RHI/IGraphicsContext.h"

#include "Renderer/RenderSystem.h"
#include "Renderer/RenderSystemDesc.h"
#include "Renderer/RenderView.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/ShaderSource.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Opaax
{
    namespace
    {
        // Bridge the module's injected log onto the engine logger — the ONLY place render-module
        // output crosses back into the host's logging. The RenderSystem itself never calls OPAAX_LOG.
        void RenderLogShim(ERenderLogLevel InLevel, const char* InMsg)
        {
            const char* lMsg = InMsg ? InMsg : "";
            switch (InLevel)
            {
                case ERenderLogLevel::Trace: OPAAX_LOG(LogRendererManager, Trace, "{}", lMsg) break;
                case ERenderLogLevel::Info:  OPAAX_LOG(LogRendererManager, Info,  "{}", lMsg) break;
                case ERenderLogLevel::Warn:  OPAAX_LOG(LogRendererManager, Warn,  "{}", lMsg) break;
                case ERenderLogLevel::Error: OPAAX_LOG(LogRendererManager, Error, "{}", lMsg) break;
            }
        }
    }

    // =========================================================================
    // CTORS - DTORS
    // =========================================================================
    RendererManager::RendererManager()  = default;
    RendererManager::~RendererManager() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool RendererManager::Startup()
    {
        // Resolve host state (the adapter's job) and pack it into a plain desc for the module.
        IConfigSystem&            lConfigSys = OpaaxApplication::GetAppService<IConfigSystem>();
        const EngineConfigData&   lEngineCfg = lConfigSys.Get<Config_Engine>().Data();
        const RendererConfigData& lRenderCfg = lConfigSys.Get<Config_Renderer>().Data();

        Window*           lWindow  = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow();
        IGraphicsContext* lSurface = lWindow ? lWindow->GetGraphicsContext() : nullptr;
        if (lSurface == nullptr)
        {
            OPAAX_LOG(LogRendererManager, Error, "No graphics context/surface for the render system")
            return false;
        }

        // Host reads the shader off disk (module never touches IPaths / file IO).
        const OpaaxString lShaderPath =
            OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Shaders/Sprite.glsl");

        RenderSystemDesc lDesc;
        lDesc.Backend      = RenderAPI::BackendFromString(lEngineCfg.RenderBackend);
        lDesc.Surface      = lSurface;
        lDesc.Width        = lWindow->GetWidth();
        lDesc.Height       = lWindow->GetHeight();
        lDesc.Log          = &RenderLogShim;
        lDesc.SpriteShader = ShaderSource::LoadShaderDescFromFile(lShaderPath);
        lDesc.ClearColor   = lRenderCfg.ClearColor;

        m_RenderSystem = MakeUnique<RenderSystem>();
        if (!m_RenderSystem->Init(lDesc))
        {
            OPAAX_LOG(LogRendererManager, Error, "RenderSystem failed to initialize")
            m_RenderSystem.reset();
            return false;
        }

        OPAAX_LOG(LogRendererManager, Info, "RendererManager started ({}x{})", lDesc.Width, lDesc.Height)
        return true;
    }

    void RendererManager::Shutdown()
    {
        m_RenderSystem.reset(); // ~RenderSystem = WaitIdle + teardown while the window/context is alive
        OPAAX_LOG(LogRendererManager, Info, "RendererManager shutdown")
    }

    // =========================================================================
    // Frame — drive the module: resize (polled), then a single scene with the test quad.
    // The BeginFrame/EndFrame(+Present) bracket owns the frame; the app loop no longer swaps.
    // =========================================================================
    void RendererManager::Render(double /*Alpha*/)
    {
        if (!m_RenderSystem)
        {
            return;
        }

        // No resize event system yet — poll the window size each frame (the documented bridge).
        Uint32 lWidth = 0, lHeight = 0;
        if (Window* lWindow = OpaaxApplication::GetAppService<IWindowManager>().GetMainWindow())
        {
            lWidth  = lWindow->GetWidth();
            lHeight = lWindow->GetHeight();
            m_RenderSystem->Resize(lWidth, lHeight);
        }
        if (lWidth == 0 || lHeight == 0) { return; }

        // Centered Y-up ortho: world (0,0) at screen centre, 1 unit = 1px. A camera-view system
        // will produce this RenderView later; for now the adapter builds it.
        const float lHalfW = static_cast<float>(lWidth)  * 0.5f;
        const float lHalfH = static_cast<float>(lHeight) * 0.5f;
        RenderView lView;
        lView.ViewProjection = glm::ortho(-lHalfW, lHalfW, -lHalfH, lHalfH, -1.f, 1.f);
        lView.Viewport       = Viewport{ 0, 0, lWidth, lHeight };

        m_RenderSystem->BeginFrame();
        m_RenderSystem->BeginScene(lView);
        // NOTE: placeholder test draw until world/scene rendering lands — a yellow 200px square.
        m_RenderSystem->GetRenderer2D().DrawQuad(Vector2F(0.f, 0.f), Vector2F(200.f, 200.f), Vector4F(1.f, 1.f, 0.f, 1.f));
        m_RenderSystem->EndScene();
        m_RenderSystem->EndFrame();
    }
}
