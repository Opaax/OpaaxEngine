#include "RenderPipeline.h"

#include "World/RenderContext.h"

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
        const RenderContext lContext{ InTarget, InAlpha };
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
