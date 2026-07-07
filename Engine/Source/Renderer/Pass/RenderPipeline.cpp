#include "RenderPipeline.h"

#include "World/RenderContext.h"
#include "Renderer/Renderer2D.h"
#include "RHI/RenderCommand.h"

#include <utility>

namespace Opaax
{
    void RenderPipeline::AddPass(UniquePtr<IRenderPass> InPass)
    {
        if (InPass)
        {
            m_Passes.push_back(std::move(InPass));
        }
    }

    void RenderPipeline::Execute(IRenderTarget& InTarget, double InAlpha)
    {
        // The frame's command buffer (opened by RenderCommand::BeginFrame in the run loop)
        // and the batch renderer are threaded to every pass through the context.
        const RenderContext lContext{ InTarget, RenderCommand::GetCommandBuffer(), InAlpha, *m_Renderer };
        for (const auto& lPass : m_Passes)
        {
            lPass->Execute(lContext);
        }
    }

    void RenderPipeline::Clear()
    {
        m_Passes.clear();
    }
}
