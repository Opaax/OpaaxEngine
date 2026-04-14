#include "RenderCommand.h"

#include "Core/Log/OpaaxLog.h"
#include "RHI/IRenderAPI.h"

namespace Opaax
{
    UniquePtr<IRenderAPI> RenderCommand::s_API = nullptr;
    
    void RenderCommand::Init(IRenderAPI* InAPI)
    {
        OPAAX_CORE_ASSERT(InAPI != nullptr)
        
        s_API = UniquePtr<IRenderAPI>(InAPI);
        s_API->Init();
    }

    void RenderCommand::Shutdown()
    {
        if (s_API)
        {
            s_API.reset();
            OPAAX_CORE_TRACE("RenderCommand::Shutdown() — API released.");
        }
    }

    void RenderCommand::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        s_API->SetViewport(X, Y, Width, Height);
    }

    void RenderCommand::SetClearColor(float Red, float Green, float Blue, float Alpha)
    {
        s_API->SetClearColor(Red, Green, Blue, Alpha);
    }

    void RenderCommand::Clear()
    {
        s_API->Clear();
    }

    void RenderCommand::DrawIndexed(Uint32 IndexCount)
    {
        s_API->DrawIndexed(IndexCount);
    }
}
