#include "RenderPipeline.h"

#include "World/RenderContext.h"
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
        // is threaded to every pass through the context.
        const RenderContext lContext{ InTarget, RenderCommand::GetCommandBuffer(), InAlpha };
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
