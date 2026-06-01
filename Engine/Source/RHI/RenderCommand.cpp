#include "RenderCommand.h"

#include "Core/Log/OpaaxLog.h"
#include "RHI/IRenderAPI.h"

namespace Opaax
{
    UniquePtr<IRenderAPI> RenderCommand::s_API = nullptr;
    
    void RenderCommand::Init(IRenderAPI* InAPI, IGraphicsContext& InContext)
    {
        OPAAX_CORE_ASSERT(InAPI != nullptr)

        s_API = UniquePtr<IRenderAPI>(InAPI);
        s_API->Init(InContext);
    }

    void RenderCommand::Shutdown()
    {
        if (s_API)
        {
            s_API.reset();
            OPAAX_CORE_TRACE("RenderCommand::Shutdown() — API released.");
        }
    }

    void RenderCommand::BeginFrame()
    {
        s_API->BeginFrame();
    }

    void RenderCommand::EndFrame()
    {
        s_API->EndFrame();
    }

    ICommandBuffer& RenderCommand::GetCommandBuffer()
    {
        return s_API->GetCommandBuffer();
    }

    void RenderCommand::SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height)
    {
        s_API->SetViewport(X, Y, Width, Height);
    }
}
